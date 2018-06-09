//
// Created by reza on 4/17/18.
//

#ifndef TUTSERVER_REQUEST_METHODS_H
#define TUTSERVER_REQUEST_METHODS_H

#include <account.h>
#include <jsoncpp/json/value.h>


//add method to wallet repository
Json::Value get_confidential_address(jsonParser parser, Postman& postman, void* context);
Json::Value get_accounts(jsonParser parser, Postman& postman, void* context);
Json::Value add_account(jsonParser parser, Postman& postman, void* context);
Json::Value register_account(jsonParser parser, Postman& postman, void* context);
Json::Value purge_accounts(jsonParser parser, Postman& postman, void* context);
Json::Value transacion(jsonParser parser, Postman& postman, void* context);
Json::Value confidential_tx_pour(jsonParser parser, Postman& postman, void* context);
Json::Value confidential_tx_mint(jsonParser parser, Postman& postman, void* context);
Json::Value get_registered_accounts(jsonParser parser, Postman& postman, void* context);
Json::Value get_notes(jsonParser parser, Postman& postman, void* context);
Json::Value get_balance(jsonParser parser, Postman& postman, void* context);

// this function must call before any actions regarding to processing requests.
void methods_setup(void);




#endif //TUTSERVER_REQUEST_METHODS_H
