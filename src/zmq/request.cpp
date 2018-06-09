//
// Created by mhghasemi on 3/17/18.
//


#include <src/cblib/note.h>
#include "request.h"
#include "messaging_server.h"


Keypair::Keypair(std::string keypair_json_string)
{
    Json::Reader rdr;
    Json::Value keypair_json;
    rdr.parse(keypair_json_string, keypair_json);

    key_pair.public_key = keypair_json["public_key"].asString();
    key_pair.secret_key = keypair_json["secret_key"].asString();

}

std::string Keypair::serialize()
{
    Json::Value serialized;

    serialized["secret_key"] = key_pair.secret_key;
    serialized["public_key"] = key_pair.public_key;


    Json::FastWriter fst;
    return fst.write(serialized);

}

void Keypair::generate_keypair()
{
    key_pair = zmqpp::curve::generate_keypair();
}

ZC_Keypair::ZC_Keypair(std::string keypair_json_string):recipient_key()
{

    Json::Reader rdr;
    Json::Value keypair_json;

    rdr.parse(keypair_json_string, keypair_json);
    std::istringstream keypair(keypair_json["recipient_key"].asString());

    recipient_key.Unserialize(keypair,1 , 1);

}

std::string ZC_Keypair::serialize()
{
    Json::Value serialized;
    std::ostringstream str;
    std::wstring temp_string;


    recipient_key.Serialize(str, 1, 1);

    std::copy(str.str().begin(), str.str().end(), std::back_inserter(temp_string));
    LOG_INFO << "is confidential keypair valid utf-8: " << is_valid_utf8(UnicodeToUTF8(temp_string).data());
    LOG_INFO << "confidential keypair utf-8 data: " << UnicodeToUTF8(temp_string);

    serialized["recipient_key"] = UnicodeToUTF8(temp_string);


    Json::FastWriter fst;
    return fst.write(serialized);
}

void Postman::update(std::string db_name)
{
    database& wallet_db = database::get_instance(db_name);
    std::vector<Keypair> keypair_temp;
    std::vector<ZC_Keypair> zc_keypair_temp;

    // empty filter, grab everything!
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;

    // wallet key pair used for transparent transactions
    if(!(wallet_db.query<Keypair>(keypair_temp, collection::key_pair, document, opt, "NOVALUE", false)))
    {
        LOG_DEBUG << "Keypair created";
        server_keypair.generate_keypair();
        //insert created keypair to database
        database::get_instance(db_name).add_request(server_keypair.serialize(), collector[collection::key_pair], "NOVALUE");
    }
    else{
        LOG_DEBUG << "Keypair recovered";
        server_keypair = keypair_temp[0];
    }


    // zcash key pairs used for confidntial transactions
    if(!(wallet_db.query<ZC_Keypair>(zc_keypair_temp, collection::confidential_key_pair, document, opt, "NOVALUE", false)))
    {
        LOG_DEBUG << "confidential Keypair created";

        //insert created keypair to database
        zc_keypair = ZC_Keypair();
        database::get_instance(db_name).add_request(zc_keypair.serialize(), collector[collection::confidential_key_pair], "NOVALUE");
    }
    else{
        LOG_DEBUG << "confidential Keypair recovered";
        zc_keypair = zc_keypair_temp[0];
    }
}


Postman::Postman(std::string db_name):zc_keypair(),db_name(db_name)
{
    srandom((unsigned)time(NULL));
    client = NULL;
    update(db_name);

}


zmqpp::curve::keypair Postman::get_keys()
{
    update(db_name);
    return server_keypair.get_keys();
}

ZC_Keypair Postman::get_conf_keys()
{
    update(db_name);
    return zc_keypair;
}

bool Postman::delete_keys()
{
    bsoncxx::document::view_or_value filter{};

    int numbered_deleted;
    bool is_successful = database::get_instance(db_name).delete_request(collection::key_pair, filter, &numbered_deleted);
    LOG_DEBUG << "number of keypairs deleted: " <<  numbered_deleted;

    return is_successful;
}

void Postman::s_client_socket(zmqpp::context & context) {
    if(client)
        delete client;
    LOG_INFO << "connecting to server...";
    client = new zmqpp::socket(context, zmqpp::socket_type::req);

    client->set(zmqpp::socket_option::curve_server_key, "wE+l1GAMV@?]yr[s=)EoZb[{&yP.l?QHd/D.ZHHL"); // server key hard coded
    client->set(zmqpp::socket_option::curve_public_key, server_keypair.public_key());
    client->set(zmqpp::socket_option::curve_secret_key, server_keypair.secret_key());

    client->connect ("tcp://localhost:9255");

    //  Configure socket to not wait at close time
    int linger = 0;
    client->set(zmqpp::socket_option::linger, linger);

    //TODO: set proper identity for clients(pubkey)
    //client->set(zmqpp::socket_option::identity, "identity");
}


Json::Value Postman::post(Json::Value rqst, void* context)
{
    LOG_INFO << "starting to post the request...";


    int retries_left = REQUEST_RETRIES;

    Json::FastWriter fstw;

    Json::Value reply;
    std::string requestMessage;


    requestMessage = fstw.write(rqst);

    zmqpp::context* cntxt = static_cast<zmqpp::context*>(context);
    s_client_socket(*cntxt);

    zmqpp::message msg(requestMessage.c_str());

    zmqpp::reactor reactor;

    bool expect_reply = true;


    auto socket_listener = [this, &expect_reply , &reply]()
    {
        LOG_DEBUG << "listener detected an event.";
        std::string replyMessage;
        Json::StyledWriter stwr;
        Json::Reader rdr;


            //process the response
            zmqpp::message zreply;

            client->receive(zreply);

            replyMessage = zreply.get(0);

            rdr.parse(replyMessage, reply);
            LOG_DEBUG << "response received from server: " << stwr.write(reply);

            expect_reply = false;


    };

    reactor.add(*client, socket_listener);

    client->send(msg);

        while(expect_reply)
        {

            reactor.poll(REQUEST_TIMEOUT);

            if(!expect_reply)
                break;

            if (--retries_left == 0) {
                LOG_WARNING << "server seems to be offline, abandoning";
                reply["description:"] = "server resonse failed.";
                expect_reply = false;
                break;
            } else {
                LOG_WARNING << "no response from server, retrying…";
                //  Old socket will be confused; close it and open a new one
                s_client_socket(*cntxt);
                //  Send request again, on new socket
                msg.push_back(requestMessage.c_str());
                client->send(msg);
            }

        }
    LOG_DEBUG << "posting process finished.";
    // never left behind the return part in functions that needs to return something :D
    return reply;
}

