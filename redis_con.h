#ifndef REDIS_CON_H
#define REDIS_CON_H

#include <hiredis/hiredis.h>
#include <string>
#include <iostream>
#include "config.h"

class RedisClient {
private:
    std::string ip;
    int port;
    redisContext* context;

public:
    RedisClient();
    ~RedisClient();
    std::string getIP() const { return ip; }
    int getPort() const { return port; }
    bool Connect();
    bool SetKey(const std::string& key, const std::string& value);
    bool GetKey( const std::string& key, std::string& value);
    bool DelKey(const std::string& key);
    bool Exists(const std::string& key);
    bool RenameKey(const std::string& oldkey,const std::string& newkey);
};

#endif
