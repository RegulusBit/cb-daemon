//
// Created by reza on 4/13/18.
//

#ifndef TUTSERVER_ACCOUNT_H
#define TUTSERVER_ACCOUNT_H


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
#include <unordered_map>
#include "db/db.h"
#include "src/utilities/utils.h"
#include <zmqpp/zmqpp.hpp>
#include <zmqpp/curve.hpp>
#include <zcash/JoinSplit.hpp>
#include "src/zmq/request.h"

// defining pointer to request processing functions
typedef Json::Value (*request_ptr)(jsonParser parser, Postman& postman, void* context);


class Account {
public:
    bool without_sk;
    Account();
    Account(std::string account_json_string);
    Account(std::string account_json_string, bool without_sk);
    Account(__uint64_t balance);
    __uint64_t get_balance(void);

    std::string get_pk_string(void)
    {
        return paymentAddress->get_pk().a_pk.ToString();
    }

//TODO:
//    ~Account()
//    {
//        if(paymentAddress)
//            delete paymentAddress;
//    }

    std::string serialize(void);

private:

    // uint256 Id; required to specifically point to any account
    //uint256 implementation needed

    std::string account_description;
    CBPaymentAddress* paymentAddress;
    double Balance;

};



class ServerEngine
{

public:

    ServerEngine(bool is_test);
    ServerEngine(std::string user_wallet_name, bool is_test);
    ~ServerEngine()
    {
        //TODO: memory management sucks! garbage collection mechanism needs modification
        //if(default_account)
        //delete default_account;
    }
    zmqpp::curve::keypair get_keys(void);
    ZCJoinSplit* get_zkparams(void){return pzcashParams;}

    std::string get_wallet_name()
    {
        return wallet_name;
    }

    bool delete_keys(void);
    Account& get_pool_account(void);

    bool sync_merkle(void);

    ZCIncrementalMerkleTree get_merkle(void)
    {
        if(!sync_merkle())
            LOG_ERROR << "merkle synchronization failed";

        return merkletree;
    }


private:

    bool test;
    Keypair client_keypair;
    ZCJoinSplit *pzcashParams = NULL;
    Account poolAccount;
    Account* default_account;
    std::vector<Account> Accounts;
    database* wallet_db;
    std::string wallet_name;
    ZCIncrementalMerkleTree merkletree;

    bool sync_engine(void);
    bool sync_keypair(void);
    bool fetch_zparams(void);
    bool sync_accounts(void);

};


bool update_local_accounts(std::vector<Account>& result_vector, Postman& postman, void* context, bool is_test);

#endif //TUTSERVER_ACCOUNT_H
