#include"server.h"
#include"mysql_con.h"
#include"redis_con.h"
#include <unistd.h> // 必须包含，提供 link() 和 unlink() 函数
#include <random>
#include <ctime>
#include <fcntl.h>  // 必须包含，用于 fcntl 设置非阻塞
#include <cerrno>   // 必须包含，用于 EAGAIN 和 EWOULDBLOCK 判断
tulun::WorkStealingThreadPool g_threadPool(100, 8);

// 生成指定长度的随机字符串（用于生成 share_id 和 提取码）
string GenerateRandomString(int length, bool only_digits = false) {
    const string chars = only_digits ? "0123456789" : "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string result;
    // 使用 C++11 随机数引擎
    static std::mt19937 generator(time(nullptr)); 
    std::uniform_int_distribution<int> distribution(0, chars.size() - 1);
    for (int i = 0; i < length; ++i) {
        result += chars[distribution(generator)];
    }
    return result;
}
void Con_Client::ShareFile()
{
    string fname = val["filename"].asString();
    string filepath = mypath + "/" + fname;

    // 1. 检查要分享的文件是否存在
    if(access(filepath.c_str(), F_OK) != 0) {
        Json::Value res; res["type"] = SHARE_FILE; res["status"] = "NOFILE";
        Send_Framed(res.toStyledString()); return;
    }

    // 2. 生成 6位短链接 和 4位提取码
    string share_id = GenerateRandomString(6, false); // 例: xY7bA2
    string extract_code = GenerateRandomString(4, true); // 例: 8392

    // 3. 存入 MySQL 数据库，设置 7 天后过期
    // 【注意：这里请替换为你自己的 MySQL 封装类的执行语句】
    char sql[512] = {0};
    sprintf(sql, "INSERT INTO share_links (share_id, extract_code, owner_tel, file_name, expire_time) "
                 "VALUES ('%s', '%s', '%s', '%s', DATE_ADD(NOW(), INTERVAL 7 DAY))", 
            share_id.c_str(), extract_code.c_str(), usertel1.c_str(), fname.c_str());
            
    MysqlClient mysql;
    if(mysql.ConnectServer()) 
    {
        // 🌟 调用你刚才新写的数据库函数
        if (mysql.DB_AddShare(share_id, extract_code, usertel1, fname)) {
            Json::Value res;
            res["type"] = SHARE_FILE;
            res["status"] = "OK";
            res["share_id"] = share_id;
            res["code"] = extract_code;
            Send_Framed(res.toStyledString());
        } else {
            Send_ERR();
        }
        }
}
void Con_Client::SaveShare()
{
    string share_id = val["share_id"].asString();
    string code = val["code"].asString();
    
    // 这两个变量将作为引用传给数据库函数，用来接收查询结果
    string owner_tel; 
    string fname;     

    MysqlClient mysql;
    if(mysql.ConnectServer()) 
    {
        // 1. 调用你新写的数据库校验函数
        // 如果查不到记录、密码错误或已过期，函数会返回 false
        if (!mysql.DB_GetShare(share_id, code, owner_tel, fname)) {
            Json::Value res; 
            res["type"] = SAVE_SHARE; 
            res["status"] = "INVALID"; 
            Send_Framed(res.toStyledString()); 
            return;
        }

        string root_dir = "/home/qin/桌面/myproject/download"; 
        string src_path = root_dir + "/" + owner_tel + "/" + fname;
        string dst_path = mypath + "/" + fname; // mypath 是当前转存用户的目录

        // 3. 检查当前用户是不是已经有这个名字的文件了（防止覆盖）
        if(access(dst_path.c_str(), F_OK) == 0) {
            // 如果重名，稍微改下名字，比如加上 "share_"
            dst_path = mypath + "/share_" + fname; 
        }

        if (link(src_path.c_str(), dst_path.c_str()) == 0) {
            Json::Value res; 
            res["type"] = SAVE_SHARE; 
            res["status"] = "OK";
            Send_Framed(res.toStyledString());
        } else {
            // 如果源文件物理上已经被分享者删了（虽然链接没过期），link 会失败
            Json::Value res; 
            res["type"] = SAVE_SHARE; 
            res["status"] = "MISSING"; 
            Send_Framed(res.toStyledString());
        }
    }
    else
    {
        // 数据库连接失败的情况
        Send_ERR();
    }
}
void Accept_CallBack(int c,short ev,void* arg)
{
    Server* p= static_cast<Server*>(arg);
    if(ev&EV_READ)
    {
        p->Accept_Client();
    }
}

void Read_callBack(int c,short ev,void* arg)
{
    Con_Client*ptr = static_cast<Con_Client*>(arg);
    if(ev&EV_READ)
    {
        ptr->Recv_Data();
    }
}


void Con_Client::Send_Framed(const std::string& msg)
{
    Send_Framed(msg.c_str(), msg.length());
}
void Con_Client::Send_ok()
{
    Json::Value v;
    v["status"] = "OK";
    Send_Framed(v.toStyledString());

}
void Con_Client::Send_ERR()
{
    Json::Value v;
    v["status"] = "ERR";
    Send_Framed(v.toStyledString());
}

void Con_Client::Send_Json(Json::Value& v)
{
    Send_Framed(v.toStyledString());
}
bool Con_Client::Is_Json(const char buff[])
{
    if(buff[0]=='{')
    {
        return true;
    }
    return false;

}

void Con_Client::Register()
{
    string usertel = val["usertel"].asString();
    string username = val["username"].asString();
    string userpasswd = val["passwd"].asString();
    string PATH = val["path"].asString();
    MysqlClient mysqlcli;
    if( !mysqlcli.ConnectServer())
    {
        Send_ERR();
        return;
    }

    if( !mysqlcli.DB_Register(usertel,username,userpasswd))
    {
        Send_ERR();
        return;
    }
    mypath = PATH + usertel;
    userpath = mypath;
    if(mkdir(mypath.c_str(),0775)==-1)
    {
        Send_ERR();
        return;
    }

    Send_ok();
    return;
}


void Con_Client::Login()
{
    string usertel = val["usertel"].asString();
    string passwd = val["passwd"].asString();
    string PATH = val["path"].asString();

    if( usertel.empty() || passwd.empty()) {
        Send_ERR(); return;
    }

    // 🌟 1. 获取智能指针保命
    auto self = shared_from_this();

    // 🌟 2. 扔进线程池异步执行（注意捕获所需的变量）
    g_threadPool.AddTask([self, usertel, passwd, PATH]() {
        string username = "";
        MysqlClient cli;
        
        if( !cli.ConnectServer() || !cli.DB_Login(usertel, username, passwd)) {
            self->Send_ERR();
            return;
        }

        // 修改当前对象的状态
        self->mypath = PATH + usertel;
        self->userpath = self->mypath;
        self->usertel1 = usertel;
        
        Json::Value v;
        v["status"] = "OK";
        v["username"] = username;
        self->Send_Json(v);
    });
}

void Con_Client::ShowFiles()
{
    DIR *ptr = opendir(mypath.c_str());
    if(ptr == nullptr)
    {
        cout<<"opendir err:"<<mypath<<endl;
        Send_ERR();
        return;
    }
    int nfiles = 0;
    int ndirs = 0;
    Json::Value resval;
    struct dirent *s = nullptr;
    struct stat st;//
    while((s = readdir(ptr))!=nullptr)
    {
        if(strncmp(s->d_name,".",1)==0)
        {
            continue;
        }
        string filename = mypath + "/" +s->d_name;
        if(lstat(filename.c_str(),&st)==-1)
        {
            cout<<"lstat err"<<endl;
            continue;
        }
        if(S_ISDIR(st.st_mode))
        {
            Json::Value tmp;
            tmp["filename"] = string(s->d_name);
            resval["arrdir"].append(tmp);
            ndirs++;
        }else{
            Json::Value tmp;
            tmp["filename"] = string(s->d_name);
            resval["arrfile"].append(tmp);
            nfiles++;
        }
        
    }

    closedir(ptr);
    resval["status"] = "OK";
    resval["ndirs"] = ndirs;
    resval["nfiles"] = nfiles;
    send(c,resval.toStyledString().c_str(),strlen(resval.toStyledString().c_str()),0);

}

std::string GetUniqueFilename(const std::string& dir, const std::string& original_name) {
    std::string full_path = dir + "/" + original_name;

    // 如果文件不存在，直接返回原名
    if (access(full_path.c_str(), F_OK) != 0)
        return original_name;

    // 分离文件名与扩展名
    std::string name = original_name;
    std::string ext = "";

    size_t dot_pos = original_name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        name = original_name.substr(0, dot_pos);
        ext = original_name.substr(dot_pos);
    }

    // 循环查找可用文件名
    int count = 1;
    std::string new_name;
    do {
        new_name = name + "(" + std::to_string(count) + ")" + ext;
        full_path = dir + "/" + new_name;
        count++;
    } while (access(full_path.c_str(), F_OK) == 0);

    return new_name;
}


void Con_Client::MkDir()
{
    string dname = val["dirname"].asString();
    string fin_dname = GetUniqueFilename(mypath,dname);
    string filepath = mypath +"/" +fin_dname;
    if(mkdir(filepath.c_str(),0775)==-1)
    {
        Send_ERR();
        return;
    }
    Send_ok();
    return;
}
#include <openssl/md5.h>
#include <iomanip>
#include <sstream>
#include <fstream>

// 计算本地文件的 MD5
std::string CalcFileMD5_Server(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    char buf[1024 * 16]; // 16KB buffer
    while (file.read(buf, sizeof(buf))) {
        MD5_Update(&md5Context, buf, file.gcount());
    }
    MD5_Update(&md5Context, buf, file.gcount());
    
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    std::ostringstream hexStream;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        hexStream << std::hex << std::setfill('0') << std::setw(2) << (int)result[i];
    }
    return hexStream.str();
}
void Con_Client::RmFile()
{
    string filename = val["filename"].asString();
    string filepath = mypath + "/" + filename;

    if(access(filepath.c_str(), F_OK) == -1) {
        Send_Framed("ERR"); return;
    }

    struct stat st;
    if(stat(filepath.c_str(), &st) == -1) {
        Send_Framed("ERR"); return;
    }

    if (S_ISDIR(st.st_mode)) {
        if (rmdir(filepath.c_str()) == 0) Send_Framed("OK");
        else Send_Framed("ERR");
        return;
    }

    // ==========================================
    // 🌟 将极其耗时的 MD5 计算与 Redis 操作丢给线程池
    // ==========================================
    if (st.st_nlink == 1) 
    {
        auto self = shared_from_this();
        g_threadPool.AddTask([self, filepath]() {
            string file_md5 = CalcFileMD5_Server(filepath);
            if (!file_md5.empty()) {
                string global_md5_key = "FileMD5:" + file_md5;
                RedisClient redis;
                if (redis.Connect()) {
                    redis.DelKey(global_md5_key);
                    cout << "【GC回收】最后一份文件被删除，清理 Redis 全局缓存: " << global_md5_key << endl;
                }
            }
        });
    } 
    else 
    {
        cout << "【安全删除】当前文件被其他用户秒传引用，仅断开链接。剩余引用数: " << (st.st_nlink - 1) << endl;
    }

    // unlink 是轻量级系统调用，直接在主线程断开硬链接即可
    if (unlink(filepath.c_str()) == 0) {
        Send_ok();
    } else {
        Send_ERR();
    }
}

void Con_Client::ReName()
{
    string s_name = val["sname"].asString();
    string t_name = val["tname"].asString();
    if (s_name.empty() || t_name.empty()) {
        Json::Value res;
        res["type"] = MVNAME;
        res["status"] = "ERR"; 
        Send_Framed(res.toStyledString());
        return;
    }
    string s_filepath = mypath + "/" +s_name;// /home/.../15209165722/wl
    string t_filepath = mypath + "/" +t_name;


    if(access(t_filepath.c_str(),F_OK)==0)
    {
        Json::Value res;
        res["type"] = MVNAME;
        res["status"] = "EXIT";
        Send_Framed(res.toStyledString());
        return;
    }

    if(rename(s_filepath.c_str(),t_filepath.c_str())!=0)
    {
        Send_ERR(); 
        return;
    }

    Send_ok();
}

void Con_Client::ChDir()
{
    string dname = val["dirname"].asString();
    string testpath = mypath+"/"+dname;
    if(access(testpath.c_str(),F_OK)==-1)
    {
        Send_ERR();
        return;
    }
    DIR *ptr = opendir(testpath.c_str());
    if(ptr ==nullptr)
    {
        Send_ERR(); 
        return;
    }
    mypath = testpath;
    
    closedir(ptr);
    Send_ok();
    return;
}

void Con_Client::Ret()
{
    if(userpath==mypath)
    {
        Send_ok();
        return;
    }
    size_t pos = mypath.find_last_of("/");
    if(pos==string::npos)
    {
        Send_ERR();
        return;
    }
    mypath = mypath.substr(0,pos);
    Send_ok();
}

void Con_Client::MvFile()
{
    string src = val["src"].asString();
    string dst = val["dst"].asString();

    string src_path = mypath+"/"+src;///home/qin/桌面/myproject/download/15209165722 + / + wl/a.txt
    string dst_path = mypath+"/"+dst;///home/qin/桌面/myproject/download/15209165722 + / + a.txt
    
    if(access(src_path.c_str(),F_OK)!=0)
    {
        Json::Value res;
        res["type"] = MVFILE;
        res["status"] = "NOFILE";
        Send_Framed(res.toStyledString());
        return;
    }

    if(access(dst_path.c_str(),F_OK)==0)
    {
        Json::Value res;
        res["type"] = MVFILE;
        res["status"] = "EXIT";
        Send_Framed(res.toStyledString());
        return;
    }

    if(rename(src_path.c_str(),dst_path.c_str())==-1)
    {
        Send_ERR();
        return;
    }

    Json::Value res;
    res["type"] = MVFILE;
    res["status"] = "OK";
    Send_Framed(res.toStyledString());

}



void Con_Client::GetFile(char *ptr)
{
    char *status = strtok_r(NULL, " ", &ptr);
    if(status == nullptr) {
        Send_Framed("ERR");
        return;
    }

    if(strcmp(status, "start") == 0)
    {
        char* fname = strtok_r(NULL, " ", &ptr);
        string pathname = mypath + "/" + fname;
        
        // 防御：清理残留的文件描述符
        if(this->fd != -1) {
            close(this->fd);
            this->fd = -1;
        }

        this->fd = open(pathname.c_str(), O_RDONLY);
        if(this->fd == -1) {
            Send_Framed("ERR");
            return;
        }

        long long filesize = lseek(this->fd, 0, SEEK_END);
        lseek(this->fd, 0, SEEK_SET);
        this->current_download_offset = 0;
        string r_str = "OK " + to_string(filesize);
        Send_Framed(r_str);   
    }
    else if(strcmp(status, "continue") == 0)
    {
        auto self = shared_from_this();
        int fd = this->fd;
        
        // 🌟 1. 主线程计算这次该从哪里读，读多少
        long long offset = this->current_download_offset;
        int read_size = 4096;
        this->current_download_offset += read_size; // 主线程向前推进指针

        // 🌟 2. 把读盘发包任务交给线程池
        g_threadPool.AddTask([self, fd, offset, read_size]() {
            
            char buff[4096] = {0};
            // 使用 pread，从绝对位置读取，线程安全！
            int num = pread(fd, buff, read_size, offset);
            
            if(num > 0) {
                // self->Send_Framed 内部有 send_mtx 锁保护，并发发包绝对安全
                self->Send_Framed(buff, num); 
            } 
            else if(num == 0) {
                self->Send_Framed("EOF");      
                // 注意：fd 的关闭和释放最好由主线程统一管理，或者确保没有后续读操作再关
            } 
            else {
                self->Send_Framed("ERR");
            }
        });
    }
    else if(strcmp(status, "stop") == 0)
    {
        if(this->fd != -1) {
            close(this->fd);
            this->fd = -1;
        }
        Send_Framed("OK");
    }
}



void Con_Client::UpFile(char *ptr)
{
    // 1. 解析客户端传来的指令参数
    char* filename = strtok_r(ptr, " ", &ptr);
    char* filesize_str = strtok_r(NULL, " ", &ptr);
    char* filemd5 = strtok_r(NULL, " ", &ptr);
    char* usertel = strtok_r(NULL, " ", &ptr);

    if (filename == nullptr || filesize_str == nullptr || filemd5 == nullptr || usertel == nullptr) {
        Send_Framed("ERR");
        return;
    }
    int filesize = atoi(filesize_str);

    // 2. 连接 Redis
    RedisClient redis;
    if (!redis.Connect()) {
        cout << "Redis 连接失败！" << endl;
        Send_Framed("ERR");
        return;
    }

    // 3. 路径解析（拼接出最终保存的绝对路径）
    string utel = usertel;
    size_t pos = mypath.find(utel);
    string subpath, fname;
    if(pos != string::npos) {
        subpath = mypath.substr(pos+utel.length());
        if (!subpath.empty() && subpath[0] == '/') {
            subpath = subpath.substr(1);
        }
        fname = subpath + "/" + filename;
    } else { 
        fname = filename;
    }

    string pathname = mypath + "/" + filename; // 目标文件的完整绝对路径

    // =========================================================
    // 🌟 核心改动 1：分离全局 Key 和 私有 Key
    // =========================================================
    // 全局秒传池 Key (纯 MD5，不区分用户)
    string global_md5_key = "FileMD5:" + string(filemd5); 
    // 个人断点续传 Key (包含手机号，隔离并发干扰)
    string resume_key = "Resume:" + string(filemd5) + ":" + utel; 

    // =========================================================
    // 🌟 核心改动 2：硬链接秒传逻辑
    // =========================================================
    string exist_file_path;
    if (redis.GetKey(global_md5_key, exist_file_path)) 
    {
        // 防御性编程：如果当前用户的目录下已经有同名的残缺文件，先删掉它
        if (access(pathname.c_str(), F_OK) == 0) {
            unlink(pathname.c_str());
        }

        // 尝试建立硬链接 (将目标路径指向已存在的物理文件)
        if (link(exist_file_path.c_str(), pathname.c_str()) == 0) {
            cout << "【秒传成功】利用硬链接完成零拷贝！文件: " << fname << endl;
            Send_Framed("EXIST");
            return; // 秒传完成，直接结束函数，不进入接收状态机
        } else {
            cout << "硬链接建立失败，降级为普通上传流程..." << endl;
        }
    }

    // =========================================================
    // 🌟 核心改动 3：私有断点续传逻辑
    // =========================================================
    int resume_pos = 0;
    string s_pos;
    if (redis.GetKey(resume_key, s_pos)) {
        resume_pos = atoi(s_pos.c_str());
        cout << "【断点续传】检测到历史进度: " << resume_pos << " 字节" << endl;
    }

    // 告诉客户端从哪里开始传
    Send_Framed(to_string(resume_pos));

    // =========================================================
    // 4. 打开文件并设置状态机上下文
    // =========================================================
    int fd = open(pathname.c_str(), O_CREAT | O_WRONLY, 0664);
    if(fd == -1) {
        Send_Framed("ERR");
        return;
    }
    lseek(fd, resume_pos, SEEK_SET);

    // 保存上下文，把舞台交给 Recv_Data 去接收纯二进制流
    this->current_file_fd = fd;
    this->expected_file_size = filesize;
    this->received_file_size = resume_pos;
    this->actual_written_size = resume_pos;//工作线程池（磁盘层）真正已经成功写入磁盘的总字节数
    //因为多线程乱序写盘，只有当这个变量的值达到 expected_file_size 时，此时才能安全地关闭文件，然后写 Redis。

    this->current_file_md5 = filemd5;
    this->current_fname = fname;
    this->current_resume_key = resume_key;           // 用于 Recv_Data 频繁更新和最后删除
    this->current_global_md5_key = global_md5_key;   // 用于 Recv_Data 最后写入全局池
    this->current_full_path = pathname;              // 用于 Recv_Data 最后写入全局池
    
    this->is_receiving_file = true; // 切换状态机，开始拦截文件流
}

void Con_Client::Recv_Data()
{
    // 🌟 ET 改造核心：必须循环读取，直到内核缓冲区掏空
    while(true) 
    {
        char buff[4096] = {0};
        int n = recv(c, buff, sizeof(buff), 0);
        
        if(n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            
            // 🌟 核心改造 1：使用本对象保存的 m_server 指针通知服务器清理自己
            m_server->RemoveClient(this->c); 
            return;
        } else if(n == 0) {
            // 客户端主动断开
            m_server->RemoveClient(this->c);
            return;
        }

        // 把读到的数据追加到主缓冲区
        recv_buffer.append(buff, n);

        // 🌟 防内存撑爆：每次读到数据后，立刻启动状态机去消耗数据
        while(!recv_buffer.empty())
        {
            if(is_receiving_file)
            {
                long long need = expected_file_size - received_file_size;
                long long write_len = std::min((long long)recv_buffer.size(), need);

                // 🌟 核心改造 2：切断数据，准备给线程池
                std::string chunk_to_write = recv_buffer.substr(0, write_len);
                recv_buffer.erase(0, write_len);

                // 主线程立刻记录这块数据该写在哪个绝对偏移量，并推进网络接收进度
                long long current_offset = received_file_size;
                received_file_size += write_len;

                // 提取投递给线程池所需的上下文（全部按值捕获，安全跨线程传递）
                auto self = shared_from_this(); // 保命神针
                int fd_to_write = current_file_fd;
                
                string resume_key = current_resume_key;
                string global_key = current_global_md5_key;
                string full_path = current_full_path;
                string fname = current_fname;
                string file_md5 = current_file_md5;
                long long expected_total = expected_file_size;

                // 🌟 核心改造 3：将耗时的写磁盘与 Redis 操作打包投入工作窃取线程池
                g_threadPool.AddTask([self, fd_to_write, chunk_to_write, current_offset, 
                                      resume_key, global_key, full_path, fname, file_md5, expected_total]() {
                    
                    // 1. 使用 pwrite 异步带偏移量写盘，绝对不会乱序或覆盖！
                    pwrite(fd_to_write, chunk_to_write.data(), chunk_to_write.size(), current_offset);

                    // 2. 累加真正落盘的进度（原子操作返回累加后的最新值）
                    long long current_written = (self->actual_written_size += chunk_to_write.size());

                    // 3. 完美同步时序：判断当前线程是不是完成“最后一块拼图”的那个幸运儿？
                    if (current_written >= expected_total)
                    {
                        // 关闭文件描述符
                        close(fd_to_write);

                        // 耗时的同步网络 I/O 操作：存入 Redis
                        RedisClient redis;
                        if (redis.Connect()) {
                            redis.DelKey(resume_key);
                            redis.SetKey(global_key, full_path);
                        }
                        cout << "上传完成:" << fname << " | MD5存储:" << file_md5 << endl;
                        
                        // 注意：如果客户端在上传完成后还需要一个通知，这里可以写 self->Send_Framed("UPLOAD_OK");
                    }
                });

                // 🌟 核心改造 4：主线程状态机的收尾逻辑
                if(received_file_size >= expected_file_size)
                {
                    is_receiving_file = false;
                    current_file_fd = -1;  // 防御性置空，文件关闭由最后一个线程池任务负责
                    actual_written_size = 0; // 重置原子计数器，为下次上传准备
                }
            }
            else
            {
                // ==========================================
                // 解析协议头与指令流部分 (保持不变)
                // ==========================================
                if(recv_buffer.size() < 4) break; 

                uint32_t net_len;
                memcpy(&net_len, recv_buffer.data(), 4);
                uint32_t msg_len = ntohl(net_len);

                if (msg_len > 10 * 1024 * 1024) { 
                    cout << "协议解析错误或包过大！" << endl;
                    m_server->RemoveClient(this->c); // 同样替换掉 delete this
                    return;
                }
                if(recv_buffer.size() < 4 + msg_len) break; 

                std::string complete_msg = recv_buffer.substr(4, msg_len);
                recv_buffer.erase(0, 4 + msg_len);
                Process_Message(complete_msg);
            }
        } // end of inner while
    } // end of outer while
}

void Con_Client::Process_Message(const std::string& msg)
{
    cout << "收到完整指令:" << msg << endl;
    
    if(Is_Json(msg.c_str()))
    {
        Json::Reader Read;
        if(!Read.parse(msg, val)) {
            cout << "json无法解析" << endl;
            Send_Framed("ERR");
            return;
        }
        int op = val["type"].asInt();
        do_run(op);
    }
    else
    {
        char buff_copy[1024];
        strncpy(buff_copy, msg.c_str(), sizeof(buff_copy)-1);
        buff_copy[sizeof(buff_copy)-1] = '\0';

        char *ptr = nullptr;
        char *s = strtok_r(buff_copy, " ", &ptr);
        if(s == nullptr) {
            Send_Framed("ERR");
            return;
        }

        if(strcmp(s, "get") == 0) {
            GetFile(ptr);
        } else if(strcmp(s, "up") == 0) {
            UpFile(ptr);
        } else {
            cout << "无法解析的操作" << endl;
            Send_Framed("ERR");
        }
    }
}


void Con_Client::do_run(int op)
{
    switch(op)
    {
        case REGISTER:
        Register();
        break;
        case LOGIN:
        Login();
        break;
        case SHOWFILES:
        ShowFiles();
        break;
        case GET:
        break;
        case POST:
        break;
        case MKDIR:
        MkDir();
        break;
        case RMFILE:
        RmFile();
        break;
        case MVNAME:
        ReName();
        break;
        case CHDIR:
        ChDir();
        break;
        case RET:
        Ret();
        break;
        case MVFILE:
        MvFile();
        break;
        case SAVE_SHARE:
        SaveShare();
        break;	
        case SHARE_FILE:
        ShareFile();
        break;
        default:
        break;
    }
}


void Server::Run()
{
    event_base_dispatch(base);
}

bool Server::Ser_Init()
{
    if(!Create_Socket())
    {
        return false;
    }
    if(!Libevent_Init())
    {
        return false;
    }
    return true;
}

bool Server::Accept_Client()
{
    int c = accept(sockfd,NULL,NULL);
    if(c<0)
    {
        return false;
    }

    int flags = fcntl(c, F_GETFL, 0);
    if (flags == -1) {
        close(c);
        return false;
    }
    if (fcntl(c, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(c);
        return false;
    }

    auto p = std::make_shared<Con_Client>(c, base,this);
    client_map[c] = p;
    struct event* c_ev = event_new(base, c, EV_READ|EV_PERSIST|EV_ET, Read_callBack, static_cast<void*>(p.get()));
    if(c_ev == nullptr)
    {
        client_map.erase(c); // 创建事件失败，安全清理
        close(c);
        return false;
    }
    p->Set_event(c_ev);
    event_add(c_ev,NULL);
    return true;


}


bool Server::Create_Socket()
{
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(-1==sockfd)
    {
        return false;
    }
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ip.c_str());
    int res = bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
    if(-1==res)
    {
        return false;
    }
    res = listen(sockfd,LIS_MAX);
    if(-1==res)
    {
        return false;
    }
    return true;



}
bool Server::Libevent_Init()
{
    base = event_init();
    if(nullptr==base)
    {
        return false;
    }
    if(-1==sockfd)
    {
        return false;
    }
    struct event* sock_ev = event_new(base,sockfd,EV_READ|EV_PERSIST,Accept_CallBack,static_cast<void*>(this));
    if(sock_ev == nullptr)
    {
        return false;
    }

    event_add(sock_ev,nullptr);
    return true;
}


int main()
{
    Server ser;
    if(!ser.Ser_Init())
    {
        cout<<"init err"<<endl;
        return 1;
    }
    ser.Run();
    return 0;

}