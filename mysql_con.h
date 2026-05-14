#include<iostream>
#include<string>
#include<string.h>
#include<unistd.h>
#include<mysql/mysql.h>
#include "config.h"

using namespace std;
class MysqlClient
{
    private:
    string dbusername;
    string dbpasswd;
    string dbip;
    string dbname;
    short dbport;
    MYSQL mysql_con;




    public:
    MysqlClient()
    {   auto conf = Config::getInstance().get("mysql");
        dbip = conf.get("ip", "127.0.0.1").asString();
        dbport = conf.get("port", 3306).asInt();
        dbusername = conf.get("user", "root").asString();
        dbpasswd = conf.get("password", "").asString();
        dbname = conf.get("dbname", "").asString();     
        
    }
    std::string getIP() const { return dbip; }
    int getPort() const { return dbport; }
    std::string getUser() const { return dbusername; }
    std::string getPassword() const { return dbpasswd; }
    std::string getDBName() const { return dbname; }
    bool ConnectServer();
    bool DB_Register(const string usertel,const string username,const string passwd);
    bool DB_Login(const string usertel ,string& username,const string passwd);
    bool DB_AddShare(const string share_id, const string code, const string usertel, const string fname);
    
    // 校验提取码，并通过引用返回源文件的拥有者手机号和文件名
    bool DB_GetShare(const string share_id, const string code, string& owner_tel, string& fname);


    ~MysqlClient()
    {
        mysql_close(&mysql_con);
    }



};

