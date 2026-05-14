#include<iostream>
#include<unistd.h>
#include<string>
#include <atomic>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unordered_map>
#include<event.h>//libevent.h
#include <jsoncpp/json/json.h>
#include<dirent.h>
#include<fcntl.h>
#include<sys/stat.h>
#include <regex>
#include <memory>
#include <mutex>
#include "WorkStealingPool.hpp"
#include "config.h"
extern tulun::WorkStealingThreadPool g_threadPool;

using namespace std;
const int LIS_MAX = 20;


enum OPS_TYPE
{
    USEREXIT = 0,
    REGISTER,
    LOGIN,
    SHOWFILES,
    GET,
    POST,
    MKDIR,
    RMFILE,
    MVNAME,
    CHDIR,
    RET,
    MVFILE,
    SAVE_SHARE,
    SHARE_FILE
};
class Con_Client;

class Server
{
    private:
    int sockfd;
    string ip;
    short port;
    struct event_base* base;

    bool Create_Socket();
    bool Libevent_Init();



    public:
    unordered_map<int, std::shared_ptr<Con_Client>> client_map;
    void RemoveClient(int fd) {
        client_map.erase(fd);
    }
    Server()
    {
        cout<<"Sever"<<endl;
        auto conf = Config::getInstance().get("server");
        ip = conf.get("ip", "127.0.0.1").asString();
        port = conf.get("port", 6000).asInt();
        this->sockfd = -1;  
        this->base = nullptr;
    }
    std::string getIP() const { return ip; }
    int getPort() const { return port; }

    Server(string ip,short port)
    {
        this->ip = ip;
        this->port = port;
        this->sockfd=-1;
        base = nullptr;
        
    }
    bool Ser_Init();
    bool Accept_Client();
    void Run();

    ~Server()
    {
        cout<<"~Server"<<endl;
        close(sockfd);
        if(base!=nullptr)
        {
            event_base_free(base);
        }

    }

};
class Con_Client: public std::enable_shared_from_this<Con_Client>
{
    private:
    std::mutex send_mtx;
    int c;
    int fd =-1;

    struct event* ev;
    struct event_base* base;
    Server* m_server;

    Json::Value val;

    string mypath;
    string userpath;
    string usertel1;

    std::string recv_buffer;     // 应用层接收缓冲区，用来"攒"数据
    
    bool is_receiving_file = false; // 状态机：当前是否处于接收文件内容的模式
    int current_file_fd = -1;       // 当前正在写入的文件描述符
    long long expected_file_size = 0; // 期望接收的文件总大小
        /*   tips1 */
    
    // 🌟 必须新增：工作线程：真正写入磁盘的字节数（原子变量，保证多线程累加绝对安全）
    std::atomic<long long> actual_written_size{0};
    /*   tips1 为了将上传文件时把md5值存入redis，进行操作通过设置期待文件长度和已经上传文件长度来判断是否应该把上传md5值到redis任务放入线程池*/

    long long received_file_size = 0; // 已经接收的文件大小
    std::string current_file_md5;   // 正在接收的文件的MD5 (用于结束后存入Redis)
    std::string current_resume_key; // Redis断点续传的Key
    std::string current_global_md5_key;    // Redis秒传的Key
    std::string current_full_path;
    std::string current_fname;

    public:
    long long current_download_offset = 0;
    void Process_Message(const std::string& msg); // 处理一条完整指令的新函数
    void Send_Framed(const char* data,uint32_t len)
    {
        uint32_t net_len = htonl(len);
        std::string packet;
        packet.append((char*)&net_len, 4);
        packet.append(data, len); 

        // 加锁保护：防止多线程同时写一个 fd 导致包错乱
        std::lock_guard<std::mutex> lock(send_mtx);
        send(c, packet.c_str(), packet.size(), 0);
    }

    void Send_Framed(const std::string& msg);
    Con_Client(int c, struct event_base* base, Server* server)
    {
        this->base = base;
        this->c = c;
        this->m_server = server; // 保存指针
    }
    void Set_event(struct event* ev)
    {
        this->ev = ev;
    }
    ~Con_Client(){
        if(ev!=nullptr)
        {
            event_free(ev);
        }
        close(c);
        cout<<"client close"<<endl;
    }


    void Recv_Data();
public:
    bool Is_Json(const char buff[]); 
    void do_run(int op);
    void Send_ok();
    void Send_ERR();
    void Send_Json(Json::Value& v);
    void ShareFile();
    void SaveShare();
    void Register();
    void Login();
    void ShowFiles();
    void GetFile(char *ptr);
    void UpFile(char *ptr);
    //void UpFile(const char* filename,int filesize,const char* filemd5,const char* usertel);
    void MkDir();
    void RmFile();
    void ReName();
    void ChDir();
    void Ret();
    void MvFile();
    
    
    

};