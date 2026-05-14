// config.h
#pragma once
#include <jsoncpp/json/json.h>
#include <fstream>
#include <string>
#include <stdexcept>

class Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    Json::Value get(const std::string &key) const {
        return root[key];
    }

private:
    Config() {
        std::ifstream ifs("config.json");
        if (!ifs.is_open()) {
            throw std::runtime_error("无法打开配置文件 config.json");
        }
        Json::Reader reader;
        if (!reader.parse(ifs, root)) {
            throw std::runtime_error("解析配置文件失败");
        }
    }

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    Json::Value root;
};
