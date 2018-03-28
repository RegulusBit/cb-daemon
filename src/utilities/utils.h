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
#include <chrono>
#include "address.h"

void EnvironmentSetup(pid_t& pid , pid_t& sid);

class Account {
private:

    // uint256 Id; required to specifically point to any account
    //uint256 implementation needed

    std::string Account_Description;
    PaymentAddress paymentAddr;
    __uint64_t Balance;

public:
    Account();
    Account(__uint64_t balance);

    __uint64_t get_balance(void);
};


class Wallet
{
private:
    Account* default_account;
    std::vector<Account* > Accounts;
public:

    Wallet();

    void add_account(Account& adding_account);


};


#endif