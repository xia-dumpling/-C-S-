#include <iostream>//输入输出流
#include <stdlib.h>//内存分配
#include <unistd.h>//unix常量类型
#include <string.h>//字符串操作函数
#include <string>//C++字符串
#include <sys/socket.h>//套接字函数
#include <arpa/inet.h>//ip地址函数
#include <netinet/in.h>//网络域地址函数和结构i体
#include <jsoncpp/json/json.h>//解析生成json格式
#include <fcntl.h>//文件操作
#include <termios.h>
#include <limits>
#include <openssl/md5.h>
#include <fstream>
#include "config.h"

using namespace std;

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


class Client
{
public:
    std::string getIP() const { return ip; }
    int getPort() const { return port; }
    std::string getDownloadPath() const { return Path; }
    Client()//构造函数初始化
    {
        auto conf = Config::getInstance().get("client");
        ip = conf.get("ip", "127.0.0.1").asString();
        port = conf.get("port", 6000).asInt();
        Path = Config::getInstance().get("path").asString();
        sockfd = -1;
        flg = false;
        op = -1;
    }

    Client(const string ips, short port)//构造函数 初始化客户端的对象
    {
        ip = ips;
        this->port = port;
        sockfd = -1;
        flg = false;
        op = -1;
    }
    int Recv_N(int fd, char* buf, int exact_len);
    void Send_Framed(const std::string& msg);
    bool Recv_Framed(std::string& out_msg);
    bool Connect_Ser();//与服务器连接，创建客户段连接服务器
    void Run();//客户端主循环

private:
    void print();
    void Register();
    void Login();
    void ShowFiles();
    void MkDir();
    void GetFile();
    void UpFile();
    void RmFile();
    void ReName();
    void ChDir();
    void Ret();
    void MvFile();
    void ShareFile();
    void SaveShare();

private:
    string ip;
    short port;
    string Path;
    bool flg; // 是否已经登陆
    int op;   // 用户操作选项id

    string usertel;
    string username;

    int sockfd; // 和服务通信的套接字

public:
    string usertel1;
};


