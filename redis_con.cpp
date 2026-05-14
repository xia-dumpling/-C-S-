#include "redis_con.h"

RedisClient::RedisClient() {
    auto conf = Config::getInstance().get("redis");
    ip = conf.get("ip", "127.0.0.1").asString();
    port = conf.get("port", 6379).asInt();
    context = nullptr;
}

RedisClient::~RedisClient() {
    if (context) {
        redisFree(context);
    }
}

bool RedisClient::Connect() {
    context = redisConnect(ip.c_str(), port);
    if (context == nullptr || context->err) {
        std::cerr << "Redis 连接失败: " << (context ? context->errstr : "空指针") << std::endl;
        return false;
    }
    return true;
}


bool RedisClient::GetKey(const std::string& key, std::string& value) {
    redisReply* reply = (redisReply*)redisCommand(context, "GET %s", key.c_str());
    if (!reply) return false;

    // type == 4 就是 REDIS_REPLY_NIL，代表 Key 不存在，这是正常现象
    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        return false; 
    }

    // type == 1 就是 REDIS_REPLY_STRING，正常获取到数据
    if (reply->type == REDIS_REPLY_STRING && reply->str != nullptr) {
        value = reply->str;
        freeReplyObject(reply);
        return true;
    }

    freeReplyObject(reply);
    return false;
}

bool RedisClient::SetKey(const std::string& key, const std::string& value) {
    // 强制按字符串格式写入，防止 value 带有空格导致 Redis 语法报错
    redisReply* reply = (redisReply*)redisCommand(context, "SET %s %s", key.c_str(), value.c_str());
    if (!reply) return false;
    
    // type == 5 就是 REDIS_REPLY_STATUS
    bool success = (reply->type == REDIS_REPLY_STATUS && reply->str != nullptr && std::string(reply->str) == "OK");
    
    freeReplyObject(reply);
    return success;
}

bool RedisClient::RenameKey(const std::string& oldKey, const std::string& newKey) {
    redisReply* reply = (redisReply*)redisCommand(context, "RENAME %s %s", oldKey.c_str(), newKey.c_str());

    // 1. 网络断开或命令发送失败
    if (!reply) {
        return false;
    }

    bool success = false;

    // 2. 只有返回状态为 STATUS，且内容是 "OK" 才算真正成功
    if (reply->type == REDIS_REPLY_STATUS && reply->str != nullptr && std::string(reply->str) == "OK") {
        success = true;
    } 
    // 3. 如果旧 Key 不存在，Redis 会返回 ERROR (比如 "ERR no such key")
    else if (reply->type == REDIS_REPLY_ERROR && reply->str != nullptr) {
        // 如果需要调试，可以解除注释查看具体报错
        // std::cerr << "[RenameKey Redis报错]: " << reply->str << std::endl;
    }

    freeReplyObject(reply);
    
    // 修复了原来写死 return true 的 Bug
    return success; 
}

bool RedisClient::DelKey(const std::string& key) {
    redisReply* reply = (redisReply*)redisCommand(context, "DEL %s", key.c_str());
    
    // 1. 网络断开或命令发送失败
    if (!reply) {
        return false;
    }

    bool success = false;

    // 2. 严格校验返回类型。DEL 成功时返回 INTEGER 类型，代表删除成功的 Key 的数量
    if (reply->type == REDIS_REPLY_INTEGER) {
        success = (reply->integer > 0);
    }

    freeReplyObject(reply);
    return success;
}
bool RedisClient::Exists(const std::string& key) {
    redisReply* reply = (redisReply*)redisCommand(context, "EXISTS %s", key.c_str());
    if (!reply) return false;
    bool exists = (reply->integer == 1);
    freeReplyObject(reply);
    return exists;
}
