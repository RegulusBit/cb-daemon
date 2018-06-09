//
// Created by reza on 4/13/18.
//

#include "account.h"
#include "db/db.h"
#include "request_methods.h"
#include "note.h"


Account::Account():without_sk(false)
{
    paymentAddress = new CBPaymentAddress();
    Balance = 0; // default value of deposited account.
}

Account::Account(std::string account_json_string):without_sk(false)
{
    Json::Reader rdr;
    Json::Value account_json;
    rdr.parse(account_json_string, account_json);

    if(!account_json.isMember("sk"))
    {
        LOG_ERROR << "the provided string to construct account is not valid";
        LOG_ERROR << "sk is provided: " << account_json.isMember("sk");
        throw Exception("the provided string to construct account is not valid");
    }

    paymentAddress = new CBPaymentAddress(account_json.get("sk", "NOVALUE").asString());
    Balance = account_json.get("balance", 0).asDouble();

}

Account::Account(std::string account_json_string, bool without_sk):without_sk(without_sk)
{
    Json::Reader rdr;
    Json::Value account_json;
    rdr.parse(account_json_string, account_json);

    if(!account_json.isMember("pk"))
    {
        LOG_ERROR << "the provided string to construct account is not valid";
        LOG_ERROR << "pk is provided: " << account_json.isMember("pk");
        throw Exception("the provided string to construct account is not valid");
    }

    paymentAddress = new CBPaymentAddress(uint256().ToString(), account_json.get("pk", "NOVALUE").asString()); // sk is invalid in this case
    Balance = account_json.get("balance", 0).asDouble();

}

Account::Account(__uint64_t balance)
{
    paymentAddress = new CBPaymentAddress();
    Balance = balance;
}

__uint64_t Account::get_balance()
{
    return Balance;
}


std::string Account::serialize()
{
    Json::Value serialized;
    Json::Value payment_addr;
    serialized["description"] = account_description;

    // balance managed in server side.
    //serialized["balance"] = Balance;

    // for merging two serialized JSON's. searching in database is easier when
    // JSON's are not nested.
    //serialized["payment_address"] = paymentAddress->serialize();
    for(auto &k: paymentAddress->serialize().getMemberNames())
        serialized[k] = paymentAddress->serialize().get(k, "NOVALUE");

    Json::FastWriter fst;
    return fst.write(serialized);
}



ServerEngine::ServerEngine(bool is_test):client_keypair(),poolAccount(),wallet_name("walletDB")
{
    this->test = is_test;
    sync_engine();
}

ServerEngine::ServerEngine(std::string user_wallet_name, bool is_test):client_keypair(),poolAccount(),wallet_name(user_wallet_name)
{
    this->test = is_test;

    sync_engine();
}

bool ServerEngine::sync_engine()
{
    sync_keypair();
    fetch_zparams();
    sync_accounts();

    return true;
}

bool ServerEngine::sync_keypair()
{
    wallet_db = &(database::get_instance(wallet_name));
    mongocxx::options::find opt;
    std::vector<Keypair> keypair_temp;


    bsoncxx::document::view_or_value document{};
    if(!(wallet_db->query<Keypair>(keypair_temp, collection::key_pair, document, opt, "NOVALUE", false)))
    {
        LOG_DEBUG << "Keypair created";
        client_keypair.generate_keypair();
        //insert created keypair to database
        database::get_instance(wallet_name).add_request(client_keypair.serialize(), collector[collection::key_pair], "NOVALUE");
    }
    else{
        LOG_DEBUG << "Keypair recovered";
        client_keypair = keypair_temp[0];
    }

    return true;
}

bool ServerEngine::fetch_zparams()
{
    //fetch zk-snark parameters
    try {
        pzcashParams = ZCJoinSplit::Prepared("vkfile", "pkfile");
    }catch(...)
    {
        throw Exception("zk-snark parameters couldn't recover.");
        return false;
    }

    return true;
}

zmqpp::curve::keypair ServerEngine::get_keys()
{
    return client_keypair.get_keys();
}


bool ServerEngine::sync_accounts()
{
    wallet_db = &(database::get_instance(wallet_name));

    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;

    if(!(wallet_db->query<Account>(Accounts, collection::accounts, document, opt, "NOVALUE", false)))
    {
        LOG_DEBUG << "account storage is empty.adding new default account";
        default_account = new Account();
        //insert default account to database
        database::get_instance(wallet_name).add_request(default_account->serialize(), collector[collection::accounts], "NOVALUE");
        LOG_DEBUG << "added default account: " << default_account->serialize();
    } else
    {
        LOG_DEBUG << "accounts extracted.";
        default_account = &Accounts[0]; // the first created account is default
        LOG_DEBUG << "recovered default account: " << default_account->serialize();
    }

}


bool ServerEngine::delete_keys()
{
    wallet_db = &(database::get_instance(wallet_name));
    bsoncxx::document::view_or_value filter{};

    int numbered_deleted;
    bool is_successful = database::get_instance(wallet_name).delete_request(collection::key_pair, filter, &numbered_deleted);
    LOG_DEBUG << "number of keypairs deleted: " <<  numbered_deleted;

    return is_successful;
}

bool ServerEngine::sync_merkle()
{

        // create or recover merkertree
        ZCIncrementalMerkleTree merkle_temp;
        std::vector<NoteCiphertext> note_temp;
        std::vector<NoteCiphertext> note_temp_reconstruct;

        if(!(query_note(note_temp, test, wallet_name)))
        {
            LOG_DEBUG << "sync_merkle: merkle created";
            //insert created merkle to database which is the default one
        }
        else{
            LOG_DEBUG << "sync_merkle: merkle recovered from database with height: " << note_temp[0].get_height();

            //reconstruct merkle tree from commitment list
            //TODO: figuring out some efficient way to save merkle tree on database
            for(int i = 1; i <= note_temp[0].get_height(); i++)
            {
                if(!query_note_one(i, note_temp_reconstruct, test, wallet_name)) {
                    LOG_ERROR << "query note with height: " << i << " failed.";
                    return false;
                }
                merkle_temp.append(note_temp_reconstruct[0].get_commitment());
                note_temp_reconstruct.clear();
            }
            merkletree = merkle_temp;
            assert(merkletree.root() == merkle_temp.root());
        }

        return true;
}


bool update_local_accounts(std::vector<Account>& result_vector, Postman& postman, void* context, bool is_test)
{
    LOG_DEBUG << "start updating local accounts based on server registered accounts.";
    bool result_state = true;
    Json::Value accounts_result;
    Json::Reader rdr;

    // create new request for server which returns registered accounts
    jsonComposer composer;
    Json::Value parameters;

    //all registered accounts of this wallet captured.
    accounts_result = get_registered_accounts(jsonParser(composer.compose("get_registered_accounts", parameters)), postman, context);

    if(accounts_result["result"] != "successful")
    {
        result_state = false;
    } else {
        // process the incoming notes
        Json::FastWriter fst;

        int i = 0;
        Json::Value array = accounts_result["response"];

        for (auto k : array) {
            i++;

            try {
                Json::Value account_json;
                rdr.parse(k.asString(), account_json);
                Account element_account(fst.write(account_json), true);
                result_vector.push_back(element_account);
            } catch (...) {
                LOG_ERROR << "error in recovering accounts.";
                result_state = false;
            }
        }
        LOG_DEBUG << "total retrieved accounts: " << i;

        if (!i) // empty list
        {
            LOG_WARNING << "accounts list received from server is empty.";
        }

    }

    //// add new accounts to database
    ////
    collection result_type;

    if(is_test)
        result_type = collection::accounts_test;
    else
        result_type = collection::accounts;

    std::vector<Account> local_result;
    std::string pk;
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;

    for(auto current_account : result_vector)
    {
        try {
            pk = current_account.get_pk_string();
            LOG_DEBUG << "adding retrieved account with pk " << pk << " to database.";
            condition.clear();
            condition.append(bsoncxx::builder::basic::kvp("pk", pk));
            if(database::get_instance(postman.get_db_name()).query<Account>(local_result, collection::accounts, condition.view(), opt,
                                                       "NOVALUE", false)) {
                LOG_WARNING << "account already registered, addition skiped.";
                continue; // skip the account that already registered n local database.
            }

        }catch(...)
        {
            LOG_ERROR << "adding recoverd accounts from server to database failed.[query phase]";
            result_state = false;
        }

        try {
            database::get_instance(postman.get_db_name()).add_request(current_account.serialize(), collector[result_type],
                                                 postman.get_keys().public_key);
        }catch(...)
        {
            LOG_ERROR << "adding recoverd accounts from server to database failed.[addition phase]";
            result_state = false;
        }
    }

    return result_state;
}





