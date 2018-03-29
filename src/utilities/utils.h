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
#include "address.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>

void EnvironmentSetup(pid_t& pid , pid_t& sid);

std::string processRequest(std::string request);

class Account {
public:
    Account();
    Account(__uint64_t balance);

    __uint64_t get_balance(void);

private:

    // uint256 Id; required to specifically point to any account
    //uint256 implementation needed

    std::string Account_Description;
    PaymentAddress paymentAddr;
    __uint64_t Balance;

};


class Wallet
{

public:

    Wallet();

    void add_account(Account& adding_account);

private:
    Account* default_account;
    std::vector<Account* > Accounts;

};

class jsonParser
{
public:

    jsonParser(std::string json);
    std::string get_header(void);
    std::string get_parameter(std::string);
private:
    Json::Value rawJson;
    std::string header;
    std::unordered_map<std::string, std::string> parameters;

};

#endif