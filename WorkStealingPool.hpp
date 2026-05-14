#include "SyncQueue_5.hpp"
#include <functional>
#include <thread>
#include <future>
#include <memory>
#include <vector>
#include <deque>
#include <atomic>
using namespace std;

#ifndef WORKSTEALING_THREADPOOL_HPP
#define WORKSTEALING_THREADPOOL_HPP
namespace tulun
{
    using TaskType = std::function<void(void)>;

    class WorkStealingThreadPool
    {
    private:
        const size_t m_numThreads;          //
        tulun::SyncQueue<TaskType> m_queue; //
        std::vector<std::shared_ptr<std::thread>> m_threadgroup;
        std::atomic_bool m_running; // true;
        std::once_flag m_flag;

    private:
        void Start(int numthreads)
        {
            if(m_running) return;
            m_running = true;
            m_threadgroup.reserve(numthreads); //reserve给m_threadgroup预留numthreads个位置
            for(int i = 0;i<numthreads;i++)
            {
                m_threadgroup.push_back(
                    std::make_shared<std::thread>(&tulun::WorkStealingThreadPool::RunInThread,this,i)
                );
            }
        }
        void RunInThread(const int index)
{
    TaskType task;

    while (m_running)
    {
        // ==========================================================
        // 1) 首先尝试从自己的桶取任务
        // ==========================================================
        int ret = m_queue.Task(&task, index);

        if (ret == 0)
        {
            try {
                task();
            } catch (...) {
                // 防止用户任务异常导致线程崩溃，捕获异常
            }
            continue;
        }
        else if (ret == -2)
        {
            // 收到停止信号
            return;
        }

        // ret == -1 => 自己桶暂时没有任务
        // ==========================================================
        // 2) 开始 Work-Stealing
        // ==========================================================
        bool stolen = false;

        for (size_t i = 1; i < m_numThreads; ++i)
        {
            if (!m_running)
                return;

            int targetIndex = (index + i) % m_numThreads;

            // 直接尝试从 targetIndex 窃取任务，不需 empty() 检查
            int stealRet = m_queue.TaskSteal(&task, targetIndex);

            if (stealRet == 0)
            {
                stolen = true;
                try {
                    task();
                } catch (...) {}
                break;  // 回到外循环再处理自己桶
            }
            else if (stealRet == -2)
            {
                return; // stop
            }
            // stealRet == -1 时，表示目标桶暂时没任务，继续尝试下一个桶
        }

        // ==========================================================
        // 3) 如果没有窃取到任务，sleep 一小段时间防止空转
        // ==========================================================
        if (!stolen)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
}

        void StopThreadGroup()
        {
            m_running = false;
            m_queue.Stop();
            for(auto& th:m_threadgroup)
            {
                if(th&&th->joinable())
                {
                    th->join();
                }
            }
            m_threadgroup.clear();
        }
        int threadIndex()
        {
            static int num = 0;
            return num++ % m_numThreads;
        }

    public:
        WorkStealingThreadPool(const int qusize = 100, const int numthreads = 8)
            :m_queue(numthreads, qusize),
            m_numThreads(numthreads),
            m_running(false)
        {
            Start(numthreads);
        }
        ~WorkStealingThreadPool()
        {
            Stop();
        }
        void Stop()
        {
            std::call_once(m_flag, [this]() {
                StopThreadGroup();
            });
        }

        void AddTask(const TaskType &task)
        {
            if (m_queue.Put(task, threadIndex()) != 0)
            {
                // cerr << "AddTask call task" << endl;
                task();
            }
        } // AddTask(std::bind(func,1,2));
        template <class Func, class... Args>
        void AddTask(Func &&func, Args &&...args)
        {
            auto task = std::make_shared<std::packaged_task<void()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
            if (m_queue.Put([task]() -> void
                            { (*task)(); }, threadIndex()) != 0)
            {
                // cerr << "AddTask call task" << endl;
                (*task)();//交给当前线程
            }
        } // AddTask(func,1,2);
        template <class Func, class... Args>
        auto submit(Func &&func, Args &&...args)
        {
            using RetType = decltype(func(args...));
            auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
            auto result = task->get_future();

            if (m_queue.Put([task]() -> void
                            { (*task)(); }, threadIndex()) != 0)
            {
                // cerr << "submit call task" << endl;
                (*task)();
            }

            return result;
        }
    };
} // namespace tulun

#endif