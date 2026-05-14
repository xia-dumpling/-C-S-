#include"mysql_con.h"

bool MysqlClient::ConnectServer()
{
    if(mysql_init(&mysql_con)==NULL)
    {
        return false;
    }
    if(mysql_real_connect(&mysql_con,dbip.c_str(),dbusername.c_str(),dbpasswd.c_str(),dbname.c_str(),dbport,NULL,0)==NULL)
    {
        return false;
    }
    return true;
}
bool MysqlClient::DB_Register(const string usertel, const string username, const string passwd)
{
    if(usertel.empty()||username.empty()||passwd.empty()) 
    {
        return false;
    }  
    string sql = "insert into User_Info values(0,'" + usertel + "','" + username + "','" + passwd + "','1',now())";

    if(mysql_query(&mysql_con,sql.c_str())!=0)
    {
        return false;
    }
    return true;
}

bool MysqlClient::DB_Login(const string usertel, string& username,const string passwd )
{
    string sql = "SELECT name, passwd FROM User_Info WHERE tel='" + usertel + "'";
    if(mysql_query(&mysql_con,sql.c_str())!=0)
    {
        return false;
    }
    MYSQL_RES * r = mysql_store_result(&mysql_con);
    if( r == nullptr)
    {
        return false;
    }

    //查看结果集是否有1条
    if( mysql_num_rows(r) == 0)//手机号码不存在,查不到记录
    {
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(r);
    string pw = row[1];
    if(passwd!=pw)
    {
        return false;
    }
    username = row[0];
    mysql_free_result(r);
    return true;
}
// 写入分享记录
bool MysqlClient::DB_AddShare(const string share_id, const string code, const string usertel, const string fname)
{
    char sql[512] = {0};
    // 拼接 SQL 语句，设置 7 天过期
    sprintf(sql, "INSERT INTO share_links (share_id, extract_code, owner_tel, file_name, expire_time) "
                 "VALUES ('%s', '%s', '%s', '%s', DATE_ADD(NOW(), INTERVAL 7 DAY))", 
            share_id.c_str(), code.c_str(), usertel.c_str(), fname.c_str());

    // 执行 SQL，0 表示成功
    if (mysql_query(&mysql_con, sql) != 0) {
        cout << "MySQL 插入分享记录失败: " << mysql_error(&mysql_con) << endl;
        return false;
    }
    return true;
}

// 校验分享并获取源文件信息
bool MysqlClient::DB_GetShare(const string share_id, const string code, string& owner_tel, string& fname)
{
    char sql[512] = {0};
    // 拼接查询 SQL
    sprintf(sql, "SELECT owner_tel, file_name FROM share_links "
                 "WHERE share_id='%s' AND extract_code='%s' AND expire_time > NOW()", 
            share_id.c_str(), code.c_str());

    if (mysql_query(&mysql_con, sql) != 0) {
        cout << "MySQL 查询分享记录失败: " << mysql_error(&mysql_con) << endl;
        return false;
    }

    // 获取查询结果集
    MYSQL_RES* res = mysql_store_result(&mysql_con);
    if (!res) {
        return false;
    }

    // 提取第一行数据
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        owner_tel = row[0]; // 对应 SELECT 的 owner_tel
        fname = row[1];     // 对应 SELECT 的 file_name
        mysql_free_result(res); // 释放结果集内存，防止内存泄漏
        return true;
    }

    // 如果没查到数据（密码错、已过期、或不存在）
    mysql_free_result(res);
    return false;
}