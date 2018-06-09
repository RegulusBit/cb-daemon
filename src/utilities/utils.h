#ifndef UTILS_H
#define UTILS_H


#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <ctime>
#include <unordered_map>
#include <chrono>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/json.h>
#include "src/cblib/address.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <src/hashidsxx/hashids.h>


bool clear_db(std::string db_name);

class Exception
{
public:
    Exception(std::string lg)
    {
        log.append(lg);
        LOG_ERROR << "what() " << lg;
    }
private:
    std::string log;
};

void EnvironmentSetup(pid_t& pid , pid_t& sid);

std::string processRequest(std::string request);


class jsonParser
{
public:
    jsonParser(){};
    jsonParser(std::string json);
    std::string get_header(void);
    std::string get_id(void);
    std::string get_wallet_name(void);
    bool get_parameter(std::string request, std::string& response);
    Json::Value get_parameters(void);
private:
    Json::Value rawJson;
    Json::Value Parameters;
    std::string header;
    std::string Id;
    std::string wallet_name;
    std::unordered_map<std::string, std::string> parameters;

};

class jsonComposer
{

public:
    jsonComposer():id_generator("salt", 16, "abcdefghijklmnopqrstuvwxyz")
    {
        // init the hashidsxx
        // this counter is used to create unique ID's for each request
        request_counter = 1;
    };

    std::string compose(std::string header, Json::Value parameters);

private:
    Json::Value rawJson;
    Json::Value Parameters;
    std::string header;
    std::string Id;
    unsigned request_counter;
    hashidsxx::Hashids id_generator;
};


#endif