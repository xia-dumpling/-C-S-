#ifndef SYNC_QUEUE_4_HPP
#define SYNC_QUEUE_4_HPP

#include <assert.h>
#include <iostream>
#include <deque>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <functional>
#include <atomic>   // 引入 atomic
#include <memory>   // 引入 unique_ptr

using namespace std;

namespace tulun
{
    const size_t MaxTaskCount = 500;
    
    template <class T>
    class SyncQueue
    {
    private:
        // ==========================================
        // 🌟 细粒度锁核心：封装“线程桶”
        // 将队列、互斥锁、条件变量绑定在一起
        // ==========================================
        struct ThreadBucket {
            std::deque<T> queue;
            mutable std::mutex mutex;
            std::condition_variable notEmpty;
            std::condition_variable notFull;
            std::condition_variable waitStop; // 每个桶专门用来等待清空的 CV
        };

        size_t m_bucketSize;                // 桶的数量 => thread num;
        size_t m_maxSize;                   // 每个桶的最大容量
        
        // 使用智能指针管理桶，因为 std::mutex 是不可复制和移动的
        std::vector<std::unique_ptr<ThreadBucket>> m_buckets; 

        int m_waitTime;  
        std::atomic<bool> m_needStop;       // 🌟 改为原子操作，支持无锁安全读取
        int m_timeout;   

        // 内部辅助函数（调用前确保已经对该桶加锁）
        bool IsFullLocked(const int index) const
        {
            return m_buckets[index]->queue.size() >= m_maxSize;
        }
        bool IsEmptyLocked(const int index) const
        {
            return m_buckets[index]->queue.empty();
        }

        template <class F>
        int Add(F &&task, const int index)
        {
            auto& bucket = m_buckets[index];
            // 🌟 核心：只锁当前操作的这个桶
            std::unique_lock<std::mutex> locker(bucket->mutex);

            bool tag = bucket->notFull.wait_for(locker, std::chrono::milliseconds(m_timeout), [this, index, &bucket]()->bool{
                return m_needStop || !IsFullLocked(index);
            });
            
            if(!tag)  // 满的，且没有要求停止
            {
                return -1;
            }
            if (m_needStop)
            {
                return -2;
            }
            
            bucket->queue.push_back(std::forward<F>(task));
            bucket->notEmpty.notify_all(); // 只唤醒等待在这个桶上的线程
            return 0;
        }

    public:
        SyncQueue(int bucketsize, int maxsize = MaxTaskCount, int timeout = 1)
            : m_bucketSize(bucketsize),
              m_maxSize(maxsize),
              m_timeout(timeout),
              m_waitTime(timeout),
              m_needStop(false) // running;
        {
            // 初始化每个桶
            for (int i = 0; i < m_bucketSize; ++i) {
                m_buckets.push_back(std::unique_ptr<ThreadBucket>(new ThreadBucket()));
            }
        }
        
        ~SyncQueue()
        {
            if (!m_needStop)
            {
                Stop();
            }
        }

        int Put(const T &task, const int index)
        {
            return Add(task, index);
        }
        int Put(T &&task, const int index)
        {
            return Add(std::forward<T>(task), index);
        }

        int Task(std::deque<T> *pdeq, const int index)
        {
            assert(pdeq != nullptr);
            auto& bucket = m_buckets[index];
            std::unique_lock<std::mutex> locker(bucket->mutex);
            
            while (!m_needStop && IsEmptyLocked(index))
            {
                auto tag = bucket->notEmpty.wait_for(locker, std::chrono::milliseconds(m_waitTime));
                if (tag == std::cv_status::timeout)
                {
                    return -1;
                }
            }
            if (m_needStop)
            {
                return -2;
            }
            *pdeq = std::move(bucket->queue);
            bucket->notFull.notify_all();
            bucket->waitStop.notify_all(); 
            return 0;
        }

        int Task(T *ps, const int index)
        {
            assert(ps != nullptr); 
            auto& bucket = m_buckets[index];
            std::unique_lock<std::mutex> locker(bucket->mutex);
            
            while (!m_needStop && IsEmptyLocked(index))
            {
                auto tag = bucket->notEmpty.wait_for(locker, std::chrono::milliseconds(m_waitTime));
                if (tag == std::cv_status::timeout)
                {
                    return -1;
                }
            }
            if (m_needStop)
            {
                return -2;
            }
            // 优化：使用 std::move 提升获取任务的效率
            *ps = std::move(bucket->queue.front());
            bucket->queue.pop_front();
            
            bucket->notFull.notify_all();
            bucket->waitStop.notify_all();
            return 0;
        }

        int TaskSteal(T* ps, const int index)
        {
            assert(ps != nullptr);
            auto& bucket = m_buckets[index];
            // 🌟 核心：窃取时，只锁目标桶
            std::unique_lock<std::mutex> locker(bucket->mutex);

            if (m_needStop)
            {
                return -2;
            }

            if (IsEmptyLocked(index))
            {
                return -1; // 没得偷
            }

            // ⭐ 从队尾偷（关键！！）
            *ps = std::move(bucket->queue.back());
            bucket->queue.pop_back();

            bucket->notFull.notify_all();
            bucket->waitStop.notify_all();

            return 0;
        }

        void Stop()
        {
            m_needStop = true;
            // 遍历并唤醒所有桶上的等待线程
            for (int i = 0; i < m_bucketSize; ++i)
            {
                auto& bucket = m_buckets[i];
                std::unique_lock<std::mutex> locker(bucket->mutex);
                bucket->notEmpty.notify_all();
                bucket->notFull.notify_all();
            }
        }

        void WaitQueueEmptyStop()
        {
            for (int i = 0; i < m_bucketSize; ++i)
            {
                auto& bucket = m_buckets[i];
                std::unique_lock<std::mutex> locker(bucket->mutex);
                while (!IsEmptyLocked(i))
                {
                    // 在各自桶的专属 waitStop 上等待
                    bucket->waitStop.wait_for(locker, std::chrono::milliseconds(m_waitTime));
                }
            }
            Stop(); // 所有桶空了之后再彻底 Stop
        }

        bool empty(const int index) const
        {
            std::unique_lock<std::mutex> locker(m_buckets[index]->mutex);
            return m_buckets[index]->queue.empty();
        }

        bool full(const int index) const
        {
            std::unique_lock<std::mutex> locker(m_buckets[index]->mutex);
            return m_buckets[index]->queue.size() >= m_maxSize;
        }

        size_t Size(const int index) const
        {
            std::unique_lock<std::mutex> locker(m_buckets[index]->mutex);
            return m_buckets[index]->queue.size();
        }

        size_t Count(const int index) const
        {
            // Count 通常不建议无锁调用，如果严格要求必须加锁
            std::unique_lock<std::mutex> locker(m_buckets[index]->mutex);
            return m_buckets[index]->queue.size();
        }

        size_t TotalTaskNum() const
        {
            size_t total = 0;
            // 统计总数时，逐个桶加锁累加
            for (int i = 0; i < m_bucketSize; ++i)
            {
                std::unique_lock<std::mutex> locker(m_buckets[i]->mutex);
                total += m_buckets[i]->queue.size();
            }
            return total;
        }

        void PrintTaskInfo() const
        {
            for (int i = 0; i < m_bucketSize; ++i)
            {
                std::unique_lock<std::mutex> locker(m_buckets[i]->mutex);
                printf("%d 桶中的任务数量: %zu \n", i, m_buckets[i]->queue.size());
            }
            printf("\n===============================\n");
        }
    };
} // namespace tulun

#endif