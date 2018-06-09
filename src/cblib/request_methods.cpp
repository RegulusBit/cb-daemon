//
// Created by reza on 4/17/18.
//

#include "request_methods.h"
#include "process_request.h"
#include <primitives/transaction.h>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <script/interpreter.h>
#include "zcash/Address.hpp"
#include "zcash/Note.hpp"
#include "zcash/Proof.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/IncrementalMerkleTree.hpp"
#include "streams.h"
#include "util.h"
#include "zcash/NoteEncryption.hpp"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "jsoncpp/json/json.h"
#include "src/cblib/note.h"
#include <algorithm>


// utilities needed by methods TODO: create independent utility cpp and headers


boost::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s)
{
    boost::array<unsigned char, ZC_MEMO_SIZE> memo = {{0x00}};

    std::vector<unsigned char> rawMemo = ParseHex(s.c_str());

    // If ParseHex comes across a non-hex char, it will stop but still return results so far.
    size_t slen = s.length();
    if (slen % 2 != 0 || (slen > 0 && rawMemo.size() != slen / 2)) {
        throw Exception("Memo must be in hexadecimal format");
    }

    if (rawMemo.size() > ZC_MEMO_SIZE) {
        char* buffer;
        sprintf(buffer, "Memo size of %d is too big, maximum allowed is %d", (char*)rawMemo.size(), ZC_MEMO_SIZE);
        throw Exception(buffer);
    }

    // copy vector into boost array
    int lenMemo = rawMemo.size();
    for (int i = 0; i < ZC_MEMO_SIZE && i < lenMemo; i++) {
        memo[i] = rawMemo[i];
    }
    return memo;
}


std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

inline bool isInteger(const std::string & s)
{
    if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;

    char * p ;
    strtol(s.c_str(), &p, 10) ;

    return (*p == 0) ;
}


Json::Value response_creator(bool is_successful, Json::Value result)
{
    Json::Value response;
    Json::FastWriter fst;

    if(is_successful)
        response["result"] = "successful";
    else
        response["result"] = "failed";

    LOG_DEBUG << "response creator: " << fst.write(result);

    response["response"] = result;

    return response;
}






bool update_confidential_balance(std::vector<NoteCiphertext> new_comming_notes, std::string db_name)
{
    LOG_DEBUG << "starting to update confidential balance.";
    bool result_state = true;




    // add each note that belong to user to the database
    for(auto current_note : new_comming_notes)
    {
        add_user_note(current_note, false, db_name);
    }

    if(!update_user_notes_status(false, db_name))
        LOG_ERROR << "update_user_notes_status failed.";
    return result_state;
}


bool update_note_repository(jsonParser parser, Postman& postman, void* context)
{
    // this method is supposed to update note lists based on server note database
    LOG_DEBUG << "starting to update notes database based on server data.";
    int requesting_height = 0;
    bool result_state = true;
    Json::Reader rdr;

    std::vector<NoteCiphertext> local_repository_result;
    std::vector<NoteCiphertext> received_repository_result;
    if(query_note(local_repository_result, false, postman.get_db_name()))
    {
       requesting_height = local_repository_result[0].get_height();  // means that we do have requesting_height already
    }
    LOG_DEBUG << "requesting from height " << requesting_height << " from server.";

    Json::Value request_note;
    Json::Value parameter;
    parameter["note_height"] = requesting_height;

    request_note["Id"] = parser.get_id();
    request_note["Method"]  = "get_notes";
    request_note["Parameters"] = parameter;

    Json::Value temp_note = postman.post(request_note, context);

    if(temp_note["result"] != "successful")
    {
        result_state = false;
    } else
    {
        // process the incoming notes
        Json::FastWriter fst;
        std::cout << fst.write(temp_note) << std::endl;

        int i = 0;
        Json::Value array = temp_note["response"];

        for(auto k : array)
        {
            i++;

            try{
                Json::Value cipher;
                rdr.parse(k.asString(), cipher);
                NoteCiphertext element_note(fst.write(cipher));
                received_repository_result.push_back(element_note);
            }catch(...) {
                LOG_ERROR << "error in recovering notes.";
            }
        }
        LOG_DEBUG << "total retrieved notes: " << i;

        if(!i) // empty list
        {
            LOG_WARNING << "note list received from server is empty.";
            // process user notes
            if(!update_confidential_balance(received_repository_result, postman.get_db_name()))
                LOG_ERROR << "update_confidential_balance failed.";
            return true;
        }
        // sorting based on height
        std::sort(received_repository_result.begin(), received_repository_result.end(), comp);

        try {
            assert(received_repository_result[0].get_height() == (requesting_height + 1));

        }catch(...) {
            LOG_ERROR << "notes received from server is not valid.";
            return false;
        }

        // add received notes to database in order
        // TODO
        int last_added = requesting_height;
        for(int i =0; i < received_repository_result.size(); i++) {
            auto k = received_repository_result[i];

            last_added++;
            if(k.get_height() != last_added) {
                result_state = false; // it can be true. this mechanism activates when notes are not in order.
                break;
            }
            std::string serialized_note = k.serialize(k.get_commitment(), k.get_nullifier() , k.get_root(), k.get_height());
            if (!database::get_instance(postman.get_db_name()).add_request(serialized_note, collector[collection::notes], "NOVALUE")) {
                result_state = false;
                break;
            }

        }

        // process user notes
        if(!update_confidential_balance(received_repository_result, postman.get_db_name()))
            LOG_ERROR << "update_confidential_balance failed.";
    }

    return result_state;
}

// methods

Json::Value get_confidential_address(jsonParser parser, Postman& postman, void* context)
{
    LOG_INFO << "start getting confidential_address to client. GET_CONFIDENTIAL_ADDRESS requested.";
    Json::Value response;
    Json::Value result;

    result["address"] = postman.get_conf_keys().get_payment_address().a_pk.ToString();
    result["pk_enc"] = postman.get_conf_keys().generate_pubkey().ToString();

    response = response_creator(true, result);
    return response;
}

Json::Value get_accounts(jsonParser parser, Postman& postman, void* context)
{
    LOG_INFO << "start getting accounts to client. GET_ACCOUNT requested.";
    Json::Value response;
    Json::Value result;
    std::vector<Account> result_vector;
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;

    std::vector<Account> all_registered_accounts; // dummy

    update_local_accounts(all_registered_accounts, postman, context, false);

    database::get_instance(postman.get_db_name()).query<Account>(result_vector, collection::accounts, document, opt, "NOVALUE", false);

   for(auto k: result_vector)
    {
        result.append(k.serialize());
    }

    response = response_creator(true, result);
    return response;
}

Json::Value get_balance(jsonParser parser, Postman& postman, void* context)
{
    LOG_INFO << "start getting wallet balance to client. GET_BALANCE requested.";
    Json::Value response;
    Json::Value result;

    update_note_repository(parser, postman, context);

    unsigned int transparent_balance = 0;
    unsigned int notes_balance = 0;

    std::vector<Account> all_registered_accounts;

    if(!update_local_accounts(all_registered_accounts, postman, context, false))
    {
        result["description"] = "getting registered accounts failed.";
        response = response_creator(false, result);
    } else
    {
        LOG_DEBUG << "local transparent accounts updated.";
        for(auto current_account : all_registered_accounts)
        {
            transparent_balance += current_account.get_balance();
        }

        notes_balance = user_notes_balance(false, postman.get_db_name());

        result["transparent_balance"] = transparent_balance;
        result["notes_balance"] = notes_balance;
        result["total balance"] = notes_balance + transparent_balance;

        response = response_creator(true, result);
    }

    return response;
}


Json::Value get_notes(jsonParser parser, Postman& postman, void* context)
{
    LOG_INFO << "start getting notes to client. GET_NOTES requested.";
    Json::Value response;
    Json::Value result;
    std::vector<NotePlaintextDB> result_vector;
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;

    update_note_repository(parser, postman, context);

    database::get_instance(postman.get_db_name()).query<NotePlaintextDB>(result_vector, collection::user_notes, document, opt, "NOVALUE", false);

    for(auto k: result_vector)
    {
        result.append(k.serialize(k.get_status(), k.get_height()));
    }

    response = response_creator(true, result);
    return response;
}

Json::Value add_account(jsonParser parser, Postman& postman, void* context)
{
    LOG_INFO << "start adding account to database. ADD_ACCOUNT requested.";
    std::string balance;
    Json::Value response;
    Json::Value result;
    Account* temp;

    // there is no response in this method: TODO: add created account as response
    result.append("");

    temp = new Account();

    if(database::get_instance(postman.get_db_name()).add_request(temp->serialize(), collector[collection::accounts], "NOVALUE"))
        response = response_creator(true, result);
    else
        response = response_creator(true, result);

    return response;
}



Json::Value purge_accounts(jsonParser parser, Postman& postman, void* context)
{
    LOG_INFO << "start purging accounts. PURGE_ACCOUNTS requested.";
    Json::Value response;
    Json::Value result;
    bsoncxx::document::view_or_value document{};
    int* deleted_numbers = new int;
    bool is_successful = database::get_instance(postman.get_db_name()).delete_request(collection::accounts, document.view(), deleted_numbers);

    result["number_of_accounts_deleted"] = *deleted_numbers;
    response = response_creator(is_successful,result);

    return response;
}

Json::Value register_account(jsonParser parser, Postman& postman, void* context) {
    LOG_INFO << "start registering account. REGISTER_ACCOUNT requested.";
    Json::Value response;
    Json::Value result;
    std::vector<Account> result_vector;
    std::string pk;
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;

    //zmqpp::context* cntxt = static_cast<zmqpp::context*>(context);
    //zmqpp::context cntxt;

    if (!parser.get_parameter("pk", pk)) {
        LOG_WARNING << "account address is not provided.";
        result["description"] = "account address is not provided.";
        response = response_creator(false, result);
    } else {

            condition.append(bsoncxx::builder::basic::kvp("pk", pk));
            database::get_instance(postman.get_db_name()).query<Account>(result_vector, collection::accounts, condition.view(), opt, "NOVALUE", false);

            if(!result_vector.size())
            {
                LOG_WARNING << "account does not found.";
                result["description"] = "account does not found.";
                response = response_creator(false, result);
            } else
            {
                Json::Value request;
                request["Id"] = parser.get_id();
                request["Method"]  = parser.get_header();
                request["Parameters"] = parser.get_parameters();

                Json::Value temp = postman.post(request, context);

                response["result"] = temp["result"];
                response["response"] = temp["response"];

            }

    }


    return response;
}

Json::Value get_registered_accounts(jsonParser parser, Postman& postman, void* context) {
    LOG_INFO << "start getting registered account. GET_REGISTERED_ACCOUNTS requested.";
    Json::Value response;
    Json::Value result;


            Json::Value request;
            request["Id"] = parser.get_id();
            request["Method"]  = parser.get_header();
            request["Parameters"] = parser.get_parameters();

            Json::Value temp = postman.post(request, context);

            response["result"] = temp["result"];
            response["response"] = temp["response"];



    return response;
}



Json::Value get_details(jsonParser parser, Postman& postman, void* context) {
    LOG_INFO << "start getting details of account. GET_DETAILS requested.";
    Json::Value response;
    Json::Value result;
    std::vector<Account> result_vector;
    std::string pk;
    bsoncxx::builder::basic::document condition;


    if (!parser.get_parameter("pk", pk)) {
        LOG_WARNING << "account address is not provided.";
        result["description"] = "account address is not provided.";
        response = response_creator(false, result);
    } else{


            Json::Value request;
            request["Id"] = parser.get_id();
            request["Method"]  = parser.get_header();
            request["Parameters"] = parser.get_parameters();

            Json::Value temp = postman.post(request, context);

            response["result"] = temp["result"];
            response["response"] = temp["response"];

    }


    return response;
}



Json::Value transacion(jsonParser parser, Postman& postman, void* context) {
    LOG_INFO << "start performing transaction. TRANSACTION requested.";
    Json::Value response;
    Json::Value result;
    std::string tx_value;
    std::string from_pk;
    std::string to_pk;


    if (!parser.get_parameter("tx_value", tx_value) || !isInteger(tx_value) || !parser.get_parameter("from_pk", from_pk) || !parser.get_parameter("to_pk", to_pk)) {
        LOG_WARNING << "transaction addresses or value is not provided.";
        result["description"] = "transaction addresses or value is not provided.";
        response = response_creator(false, result);
    } else{


        Json::Value request;
        request["Id"] = parser.get_id();
        request["Method"]  = parser.get_header();
        request["Parameters"] = parser.get_parameters();

        Json::Value temp = postman.post(request, context);

        response["result"] = temp["result"];
        response["response"] = temp["response"];

    }


    return response;
}


Json::Value confidential_tx_mint(jsonParser parser, Postman& postman, void* context) {
    LOG_INFO << "start performing confidential transaction. CONFIDENTIAL_TX_MINT requested.";
    Json::Value response;
    Json::Value result;
    std::ostringstream serialized_tx;
    CMutableTransaction tx;

    //vpub_old and vpub_new
    std::string tx_value_from;
    std::string tx_value_to;

    // transparent addresses of confidential transaction
    std::string from_pk_transparent;
    std::string to_pk_transparent;

    // confidential transaction destination address
    std::string dest_conf_address;
    std::string pk_enc;

    if (!parser.get_parameter("tx_value_to", tx_value_to) || !isInteger(tx_value_to) || !parser.get_parameter("tx_value_from", tx_value_from) || !isInteger(tx_value_from) ||
            !parser.get_parameter("from_pk_transparent", from_pk_transparent) || !parser.get_parameter("to_pk_transparent", to_pk_transparent) ||
            !parser.get_parameter("dest_conf_address", dest_conf_address) || !parser.get_parameter("pk_enc", pk_enc)) {
        LOG_WARNING << "transaction addresses or value is not provided.";
        result["description"] = "transaction addresses or value is not provided.";
        response = response_creator(false, result);
    } else {

        update_note_repository(parser, postman, context);

        // get the latest merkle root from server
        Json::Value request_merkle;
        request_merkle["Id"] = parser.get_id();
        request_merkle["Method"] = "get_merkle_root";
        request_merkle["Parameters"] = "";

        Json::Value temp_merkle = postman.post(request_merkle, context);

        if (temp_merkle["result"] != "successful") {
            response["result"] = temp_merkle["result"];
            response["response"] = temp_merkle["response"];
        } else {
            // pp: public parameters used in the network

            ZCJoinSplit *pzcashParams = NULL;
            pzcashParams = ZCJoinSplit::Prepared("vkfile", "pkfile");

            assert(pzcashParams != nullptr);

            libzcash::PaymentAddress payment_dest;
            //Address components
            try {
                    LOG_DEBUG << "confidential destination address: " << uint256S(dest_conf_address).ToString();
                    LOG_DEBUG << "confidential destination pk_enc: " << uint256S(pk_enc).ToString();
                    //payment_dest = postman.get_conf_keys().get_payment_address();
                    payment_dest = libzcash::PaymentAddress(uint256S(dest_conf_address), uint256S(pk_enc));
            }catch(...)
            {
                result["description"] = "provided confidential address is not valid.";
                response = response_creator(false, result);
                return response;
            }

            std::string memo = "TEST TRANSACTION";
            auto result_memo = get_memo_from_hex_string(string_to_hex(memo));
            LOG_DEBUG << "memo result: " << result_memo.data() << std::endl;

            // Generate an ephemeral keypair.
            uint256 joinSplitPubKey;
            unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
            crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);



            // Set up a JoinSplit description
            uint256 randomSeed;
            uint64_t vpub_old = std::stoi(tx_value_from);
            uint64_t vpub_new = std::stoi(tx_value_to);
            boost::array<uint256, 2> macs;
            boost::array<uint256, 2> nullifiers;
            boost::array<uint256, 2> commitments;

            //get root from server
            auto rt = uint256S(temp_merkle["response"]["merkle_root"].asString());
            LOG_DEBUG << "merkle root recovered from server with value: " << rt.ToString();

            boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
            libzcash::ZCProof proof;


            boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
                    libzcash::JSInput(),
                    libzcash::JSInput() // dummy input of zero value
            };
            boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
                    libzcash::JSOutput(),
                    libzcash::JSOutput(payment_dest, int(vpub_old - vpub_new))
            };

            outputs[0].memo = result_memo;
            outputs[1].memo = result_memo;

            boost::array<libzcash::Note, 2> output_notes;

            JSDescription jsec(*pzcashParams, joinSplitPubKey, rt, inputs, outputs, vpub_old, vpub_new, false);


            tx.vjoinsplit.push_back(jsec);
            tx.joinSplitPubKey = joinSplitPubKey;
            tx.nVersion = 2;



            // public input value
            tx.vin.resize(2);
            tx.vin[0].prevout.hash = uint256S(from_pk_transparent);

            //public value output
            tx.vin[1].prevout.hash = uint256S(to_pk_transparent);

            // Empty output script.
            uint32_t consensusBranchId = 0;
            CScript scriptCode;
            CTransaction signTx(tx);
            uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);

            // Add the signature
            assert(crypto_sign_detached(&tx.joinSplitSig[0], NULL,
                                        dataToBeSigned.begin(), 32,
                                        joinSplitPrivKey
            ) == 0);

            CTransaction ctx(tx);

            ctx.Serialize(serialized_tx, 1, 2);

            Json::Value request;
            request["Id"] = parser.get_id();
            request["Method"] = parser.get_header();
            Json::Value tempie;
            tempie["tx"] = serialized_tx.str();
            request["Parameters"] = tempie;

            LOG_DEBUG << "sending confidential transaction to server.";
            Json::Value temp = postman.post(request, context);

            response.clear();
            response["result"] = temp["result"];
            response["response"] = temp["response"];
        }

        // final note update
        update_note_repository(parser, postman, context);
    }

    return response;
}


bool joinsplit_creator(CAmount from_value, std::vector<JSDescription>& result, uint256 joinSplitPubKey, libzcash::PaymentAddress destination,
                       uint256 root,  uint64_t vpub_old_total, uint64_t vpub_new_total,Postman& postman, bool is_test, bool compute_proof)
{
    LOG_INFO << "joinsplit creator called. creating joinsplit for confidential transaction.";
    LOG_INFO << "creating joinsplits for value equal to: " << from_value;
    bool is_mint = false;
    CAmount to_value = 0;
    if(from_value < 0)
    {
        is_mint = true;
        to_value = (-1)*from_value;
    }


    bool final_result = true;
    bool transparent_value_included = false;

    std::string memo = "TEST TRANSACTION";
    auto result_memo = get_memo_from_hex_string(string_to_hex(memo));
    LOG_DEBUG << "memo result: " << result_memo.data() << std::endl;

    auto a_pk = postman.get_conf_keys().get_payment_address();
    auto a_sk = postman.get_conf_keys().get_recipient_key();

    // i assume that the note repository is updated version.
    ServerEngine engine(postman.get_db_name(), is_test);
    auto merkle = engine.get_merkle();

    //get root
    auto rt = root;
    assert(rt == merkle.root());

    if(!is_mint) {
        CAmount remaining_value = from_value;

        // the note repository doesn't update here, assuming it's the updated version of server db


        if (!purge_user_notes(is_test, postman.get_db_name())) {
            LOG_ERROR << "getting user balance failed.[what() pre-process purging of notes failed.]";
            final_result = false;
        } else {
            std::vector<NotePlaintextDB> result_vector;
            std::vector<NotePlaintextDB> result_vector_not_spent;



            if (query_user_note(0, result_vector, is_test, postman.get_db_name())) {

                int counter = 0;
                for (auto k : result_vector) {
                    if (k.get_status() == false) {
                        counter++;
                        result_vector_not_spent.push_back(k);
                    }
                }
                LOG_DEBUG << "unspent user notes count: " << counter;
                // now we have every active user notes that could be spent.
                std::sort(result_vector_not_spent.begin(), result_vector_not_spent.end(), comp_value);

                auto size = result_vector_not_spent.size();
                int note_pairs = int(size / 2);
                bool note_residual = size % 2;
                LOG_INFO << "creating joinsplit with " << note_pairs << " of pairs and is residual exists: "
                         << note_residual;



                for (int i = 0; i < 2 * note_pairs; i + 2) {
                    // create joinsplit
                    std::vector<libzcash::Note> input_notes;
                    std::vector<ZCIncrementalWitness> witnesses;
                    CAmount pair_value = 0;
                    for (int j = 0; j < 2; j++) {
                        libzcash::Note note(a_pk.a_pk, result_vector_not_spent[i + j].value,
                                            result_vector_not_spent[i + j].rho, result_vector_not_spent[i + j].r);
                        input_notes.push_back(note);

                        // get witness
                        ZCIncrementalWitness witness;
                        note_witness_merkle(note.cm(), rt, witness, is_test, postman.get_db_name());
                        witnesses.push_back(witness);

                        pair_value += note.value;
                    }

                    // Set up a JoinSplit description
                    uint256 randomSeed;
                    uint64_t vpub_old = 0;
                    uint64_t vpub_new = 0;
                    boost::array<uint256, 2> macs;
                    boost::array<uint256, 2> nullifiers;
                    boost::array<uint256, 2> commitments;
                    boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs;
                    boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs;
                    //include transparent values
                    if (!transparent_value_included) {
                        vpub_old = vpub_old_total;
                        vpub_new = vpub_new_total;
                        transparent_value_included = true;
                    }


                    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
                    libzcash::ZCProof proof;

                    if (pair_value >= remaining_value) {
                        inputs = {
                                libzcash::JSInput(witnesses[0], input_notes[0], a_sk),
                                libzcash::JSInput(witnesses[1], input_notes[1], a_sk)
                        };

                        outputs = {
                                libzcash::JSOutput(a_pk, int(pair_value - remaining_value)),    // the remaining value backs to a_pk itself
                                libzcash::JSOutput(destination, vpub_old + int(remaining_value) - vpub_new)
                        };

                        remaining_value = 0;
                    } else {
                        inputs = {
                                libzcash::JSInput(witnesses[0], input_notes[0], a_sk),
                                libzcash::JSInput(witnesses[1], input_notes[1], a_sk)
                        };

                        outputs = {
                                libzcash::JSOutput(),    // dummy output
                                libzcash::JSOutput(destination, int(pair_value))
                        };

                        remaining_value -= pair_value;
                    }


                    outputs[0].memo = result_memo;
                    outputs[1].memo = result_memo;

                    boost::array<libzcash::Note, 2> output_notes;
                      try {
                    JSDescription jsec(*engine.get_zkparams(), joinSplitPubKey, rt, inputs, outputs, vpub_old, vpub_new,
                                       compute_proof);
                    result.push_back(jsec);
                       }catch (...)
                      {
                          LOG_ERROR << "creating joinsplit failed.[JSDescription constructor]";
                          final_result = false;
                      }

                    if (!remaining_value)
                        break;
                }
                if (note_residual) {

                    libzcash::Note note(a_pk.a_pk, result_vector[size - 1].value, result_vector[size - 1].rho,
                                        result_vector[size - 1].r);

                    // get witness
                    ZCIncrementalWitness witness;
                    note_witness_merkle(note.cm(), rt, witness, is_test, postman.get_db_name());
                    LOG_INFO << "creating residual joinsplit with remaining value of: "
                             << int(note.value - remaining_value);
                    if (remaining_value > note.value) {
                        LOG_WARNING << "note deposit is not enough";
                        final_result = false;
                    } else {
                        // Set up a JoinSplit description
                        uint256 randomSeed;
                        uint64_t vpub_old = 0;
                        uint64_t vpub_new = 0;
                        boost::array<uint256, 2> macs;
                        boost::array<uint256, 2> nullifiers;
                        boost::array<uint256, 2> commitments;
                        boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs;
                        boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs;
                        //include transparent values
                        if (!transparent_value_included) {
                            vpub_old = vpub_old_total;
                            vpub_new = vpub_new_total;
                            transparent_value_included = true;
                        }


                        boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
                        libzcash::ZCProof proof;

                        inputs = {
                                libzcash::JSInput(witness, note, a_sk),
                                libzcash::JSInput() // dummy input
                        };

                        outputs = {
                                libzcash::JSOutput(a_pk, int(note.value - remaining_value)),    // the remaining value backs to a_pk itself
                                libzcash::JSOutput(destination, vpub_old + int(remaining_value) - vpub_new)
                        };


                        outputs[0].memo = result_memo;
                        outputs[1].memo = result_memo;

                        boost::array<libzcash::Note, 2> output_notes;

                        try {
                        JSDescription jsec(*engine.get_zkparams(), joinSplitPubKey, rt, inputs, outputs, vpub_old,
                                           vpub_new,
                                           compute_proof);
                        result.push_back(jsec);
                           }catch (...)
                           {
                                LOG_ERROR << "creating joinsplit failed.[JSDescription constructor]";
                                final_result = false;
                             }
                    }
                }


            } else {
                LOG_WARNING << "joinsplit creator result will be empty.";
                final_result = false;
            }
        }
    } else
    {
        // is_mint = true , there is no need to input notes from user repository.
        // Set up a JoinSplit description
        uint256 randomSeed;
        uint64_t vpub_old = 0;
        uint64_t vpub_new = 0;
        boost::array<uint256, 2> macs;
        boost::array<uint256, 2> nullifiers;
        boost::array<uint256, 2> commitments;
        boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs;
        boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs;
        //include transparent values
        if (!transparent_value_included) {
            vpub_old = vpub_old_total;
            vpub_new = vpub_new_total;
            transparent_value_included = true;
        }


        boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
        libzcash::ZCProof proof;

        inputs = {
                libzcash::JSInput(), // dummy input
                libzcash::JSInput() // dummy input
        };

        outputs = {
                libzcash::JSOutput(a_pk, to_value),    // the remaining value backs to a_pk itself
                libzcash::JSOutput(destination, vpub_old - to_value - vpub_new)
        };


        outputs[0].memo = result_memo;
        outputs[1].memo = result_memo;

        boost::array<libzcash::Note, 2> output_notes;

        try {
            JSDescription jsec(*engine.get_zkparams(), joinSplitPubKey, rt, inputs, outputs, vpub_old,
                               vpub_new,
                               compute_proof);
            result.push_back(jsec);
        }catch (...)
        {
            LOG_ERROR << "creating joinsplit failed.[JSDescription constructor]";
            final_result = false;
        }
    }
    return final_result;
}


Json::Value confidential_tx_pour(jsonParser parser, Postman& postman, void* context) {
    LOG_INFO << "start performing confidential transaction. CONFIDENTIAL_TX_POUR requested.";
    Json::Value response;
    Json::Value result;
    std::ostringstream serialized_tx;
    CMutableTransaction tx;

    //vpub_old and vpub_new
    std::string tx_value_from;
    std::string tx_value_to;

    // transparent addresses of confidential transaction
    std::string from_pk_transparent;
    std::string to_pk_transparent;

    // confidential transaction destination address
    std::string dest_conf_address;
    std::string pk_enc;
    std::string note_value_to;
    int note_value_from = 0;

    if (!parser.get_parameter("tx_value_to", tx_value_to) || !isInteger(tx_value_to) || !parser.get_parameter("tx_value_from", tx_value_from) || !isInteger(tx_value_from) ||
        !parser.get_parameter("from_pk_transparent", from_pk_transparent) || !parser.get_parameter("to_pk_transparent", to_pk_transparent) ||
        !parser.get_parameter("dest_conf_address", dest_conf_address) || !parser.get_parameter("pk_enc", pk_enc) ||
        !parser.get_parameter("note_value_to", note_value_to) || !isInteger(note_value_to)) {
        LOG_WARNING << "transaction addresses or value is not provided.";
        result["description"] = "transaction addresses or value is not provided.";
        response = response_creator(false, result);
    } else {


        // get the balance of notes and check if the requested note_transaction value is adequate
        update_note_repository(parser, postman, context);
        note_value_from = std::stoi(tx_value_to) + std::stoi(note_value_to) - std::stoi(tx_value_from);
        jsonComposer composer;
        Json::Value params;
        Json::Value balance_response = get_balance(jsonParser(composer.compose("get_balance", params)), postman,
                                                   context);
        if(balance_response["result"] != "successful") {
            response["result"] = balance_response["result"];
            response["response"] = balance_response["response"];
        } else if(note_value_from > std::stoi(balance_response["response"]["notes_balance"].asString())) {
            LOG_WARNING << "notes balance is not enough for performing transaction";
            result["description"] = "notes balance is not enough for performing transaction.";
            response = response_creator(false, result);
        } else if(std::stoi(tx_value_from) > std::stoi(balance_response["response"]["transparent_balance"].asString()))
        {
            LOG_WARNING << "transparent balance is not enough for performing transaction";
            result["description"] = "transparent balance is not enough for performing transaction.";
            response = response_creator(false, result);
        } else if(std::stoi(tx_value_from) < 0 || std::stoi(tx_value_to) < 0 || std::stoi(note_value_to) < 0)
        {
            LOG_WARNING << "invalid values for transaction.";
            result["description"] = "invalid values for transaction.";
            response = response_creator(false, result);
        }
        else {

            // get the latest merkle root from server
            Json::Value request_merkle;
            request_merkle["Id"] = parser.get_id();
            request_merkle["Method"] = "get_merkle_root";
            request_merkle["Parameters"] = "";

            Json::Value temp_merkle = postman.post(request_merkle, context);

            if (temp_merkle["result"] != "successful") {
                response["result"] = temp_merkle["result"];
                response["response"] = temp_merkle["response"];
            } else {

                libzcash::PaymentAddress payment_dest;
                //Address components
                try {
                    LOG_DEBUG << "confidential destination address: " << uint256S(dest_conf_address).ToString();
                    LOG_DEBUG << "confidential destination pk_enc: " << uint256S(pk_enc).ToString();
                    //payment_dest = postman.get_conf_keys().get_payment_address();
                    payment_dest = libzcash::PaymentAddress(uint256S(dest_conf_address), uint256S(pk_enc));
                } catch (...) {
                    result["description"] = "provided confidential address is not valid.";
                    response = response_creator(false, result);
                    return response;
                }

                std::string memo = "TEST TRANSACTION";
                auto result_memo = get_memo_from_hex_string(string_to_hex(memo));
                LOG_DEBUG << "memo result: " << result_memo.data() << std::endl;

                // Generate an ephemeral keypair.
                uint256 joinSplitPubKey;
                unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
                crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);



                // Set up a JoinSplit description
                uint64_t vpub_old = std::stoi(tx_value_from);
                uint64_t vpub_new = std::stoi(tx_value_to);
                //get root from server
                auto rt = uint256S(temp_merkle["response"]["merkle_root"].asString());
                LOG_DEBUG << "merkle root recovered from server with value: " << rt.ToString();

                if (joinsplit_creator(note_value_from, tx.vjoinsplit, joinSplitPubKey, payment_dest, rt, vpub_old,
                                      vpub_new, postman, false, false)) {

                    tx.joinSplitPubKey = joinSplitPubKey;
                    tx.nVersion = 2;



                    // public input value
                    tx.vin.resize(2);
                    tx.vin[0].prevout.hash = uint256S(from_pk_transparent);

                    //public value output
                    tx.vin[1].prevout.hash = uint256S(to_pk_transparent);

                    // Empty output script.
                    uint32_t consensusBranchId = 0;
                    CScript scriptCode;
                    CTransaction signTx(tx);
                    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0,
                                                           consensusBranchId);

                    // Add the signature
                    assert(crypto_sign_detached(&tx.joinSplitSig[0], NULL,
                                                dataToBeSigned.begin(), 32,
                                                joinSplitPrivKey
                    ) == 0);

                    CTransaction ctx(tx);

                    ctx.Serialize(serialized_tx, 1, 2);

                    Json::Value request;
                    request["Id"] = parser.get_id();
                    request["Method"] = parser.get_header();
                    Json::Value tempie;
                    tempie["tx"] = serialized_tx.str();
                    request["Parameters"] = tempie;

                    LOG_DEBUG << "sending confidential transaction to server.";
                    Json::Value temp = postman.post(request, context);

                    response.clear();
                    response["result"] = temp["result"];
                    response["response"] = temp["response"];
                }else
                {
                    result["description"] = "creating joinsplits failed.";
                    response = response_creator(false, result);
                }
            }
            // final note update
            if(!update_note_repository(parser, postman, context))
                LOG_ERROR << "update_note_repository failed.";
        }
    }

    return response;
}

void methods_setup(void)
{
    Repository::get_instance().add_to_repository("get_confidential_address", get_confidential_address);
    Repository::get_instance().add_to_repository("get_accounts", get_accounts);
    Repository::get_instance().add_to_repository("get_notes", get_notes);
    Repository::get_instance().add_to_repository("purge_accounts", purge_accounts);
    Repository::get_instance().add_to_repository("add_account", add_account);
    Repository::get_instance().add_to_repository("register_account", register_account);
    Repository::get_instance().add_to_repository("get_registered_accounts", get_registered_accounts);
    Repository::get_instance().add_to_repository("get_details", get_details);
    Repository::get_instance().add_to_repository("transaction", transacion);
    Repository::get_instance().add_to_repository("confidential_tx_pour", confidential_tx_pour);
    Repository::get_instance().add_to_repository("confidential_tx_mint", confidential_tx_mint);
    Repository::get_instance().add_to_repository("get_balance", get_balance);
}