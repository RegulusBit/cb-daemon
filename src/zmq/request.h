//
// Created by mhghasemi on 3/17/18.
//

#ifndef BNB_CLI_REQUEST_H
#define BNB_CLI_REQUEST_H

#include <jsoncpp/json/json.h>
#include <db/db.h>

#include <zmqpp/socket.hpp>
#include <zmqpp/context.hpp>
#include <zmqpp/socket.hpp>
#include <zmqpp/message.hpp>
#include <zmqpp/reactor.hpp>
#include <zmqpp/curve.hpp>
#include "zcash/NoteEncryption.hpp"


#define REQUEST_TIMEOUT     1500    //  msecs, (> 1000!)
#define REQUEST_RETRIES     3       //  Before we abandon



class Keypair
{

public:
    Keypair(){};
    Keypair(std::string keypair_json_string);
    std::string serialize(void);
    void generate_keypair(void);

    std::string secret_key() { return key_pair.secret_key;}
    std::string public_key() { return key_pair.public_key;}
    zmqpp::curve::keypair get_keys(void) {return key_pair;}

private:

    zmqpp::curve::keypair key_pair;
};


class ZC_Keypair
{

public:
    ZC_Keypair():recipient_key(libzcash::SpendingKey::random()){};
    ZC_Keypair(std::string keypair_json_string);
    std::string serialize(void);

    libzcash::SpendingKey get_recipient_key() { return recipient_key; }
    libzcash::PaymentAddress get_payment_address() { return  recipient_key.address();}

    uint256 generate_privkey(){ return ZCNoteEncryption::generate_privkey(recipient_key);}
    uint256 generate_pubkey(){ return ZCNoteEncryption::generate_pubkey(
                ZCNoteEncryption::generate_privkey(recipient_key));}

private:

    libzcash::SpendingKey recipient_key;
};



class Postman {
public:
    Postman(std::string db_name);
    ~Postman()
    {
        if(client)
            delete client;
    }
    Json::Value post(Json::Value rqst, void* context);
    void s_client_socket(zmqpp::context &context);
    void update(std::string db_name);

    zmqpp::curve::keypair get_keys(void);
    ZC_Keypair get_conf_keys(void);
    bool delete_keys(void);

    std::string get_db_name(void)
    {
        return db_name;
    }

private:


    zmqpp::socket *client;
    Keypair server_keypair;
    ZC_Keypair zc_keypair;
    std::string db_name;
};


#endif //BNB_CLI_REQUEST_H
