//
// Created by reza on 4/13/18.
//

#include <src/cblib/request_methods.h>
#include "cb_tests.h"
#include "zmq/request.h"
#include <thread>
#include <src/zmq/messaging_server.h>

void cb_address_tests(void)
{
    LOG_DEBUG << ">>> " << "CBPaymentAddress tests begins.";

    // create dummy spending key
    CBPaymentAddress payAddr;
    LOG_DEBUG << ">>> " << "payment address created with spending key: " << payAddr.get_sk().inner().ToString();

    // turn dummy_sk to string format for saving in database
    std::string dummy_sk_string = payAddr.get_sk().inner().ToString();

    // create another payment address from string
    CBPaymentAddress dummy_payment_second(dummy_sk_string);
    assert(payAddr.get_sk() == dummy_payment_second.get_sk());

    //serialize tests
    Json::FastWriter fst;
    LOG_DEBUG << ">>> " << "payment address serialized: " << fst.write(payAddr.serialize());
}



void cb_db_tests(void)
{
    // filter specification
    bsoncxx::document::view_or_value filter{};
    collection result_type = collection::accounts_test;
    std::vector<Account> result_vector;
    Json::StyledWriter wrt;

    // accessing test db
    mongocxx::database& test_db = database::get_instance(DEFAULT_DB).get_db();
    mongocxx::collection coll = test_db.collection(collector[result_type]);

    // drop all collection documents
    coll.drop();

    // extract every document
    mongocxx::cursor curser = coll.find(filter);
    LOG_DEBUG << ">>> " << "database successfully accessed.";

    bool has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << ">>> " << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(!has_element);
    LOG_DEBUG << ">>> " << "accounts_test has elements: " << has_element;

    // add document to database
    // create dummy account with balance value of 50
    Account test_account(50);

    //insert dummy account to database
    database::get_instance(DEFAULT_DB).add_request(test_account.serialize(), collector[result_type], "NOVALUE");

    // extract every document
    curser = coll.find(filter);
    LOG_DEBUG << ">>> " << "database successfully accessed.";

    has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << ">>> " << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(has_element);
    LOG_DEBUG << ">>> " << "after addition has elements: " << has_element;



    //non-blinded query
    // dummy account with value of 25 as a query target
    Account test_account2(25);
    database::get_instance(DEFAULT_DB).add_request(test_account2.serialize(), collector[result_type], "NOVALUE");

//    TODO: condition in stream mode doesn't work
//    bsoncxx::builder::basic::document condition;
//    condition.append(bsoncxx::builder::basic::kvp("payment_address",
//                                                  [](bsoncxx::builder::basic::document sa)
//                                                  {sa.append(bsoncxx::builder::basic::kvp("pk","pk"));}));

    bsoncxx::builder::basic::document condition;
    condition.append(bsoncxx::builder::basic::kvp("balance", 25));

    curser = coll.find(condition.view());

    has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << ">>> " << "query result: " << bsoncxx::to_json(doc);
        LOG_DEBUG << ">>> " << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(has_element);
    LOG_DEBUG << ">>> " << "query has elements: " << has_element;


    // deleting tests:
    LOG_DEBUG << ">>> " << "deleting tests starts.deleting documents of testDB";

    condition.clear();
    int* deleted_number = new int;
    assert(database::get_instance(DEFAULT_DB).delete_request(result_type, condition.view(), deleted_number));
    LOG_DEBUG << ">>> " << "number of deleted documents: " << *deleted_number;
    delete deleted_number;
    // free the test database

    // extract every document
    curser = coll.find(filter);
    LOG_DEBUG << ">>> " << "database successfully accessed.";

    has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << ">>> " << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(!has_element);
    LOG_DEBUG << ">>> " << "accounts_test has elements: " << has_element;


    coll.drop();
}

Json::Value test_method(jsonParser parser, Postman& postman , void* context)
{
    LOG_DEBUG << ">>> " << "test_method reached out.";
    Json::Value response;
    response["result"] = "successful";
    return response;
}

void cb_request_tests(void)
{

    // some useful tools
    Json::Value request;
    Json::Value response;
    Json::FastWriter fst;
    Json::Reader rdr;

    Postman postman(DEFAULT_DB);

    //TODO: list of request test cases should update with newer added methods
    //create dummy request
    request["Method"] = "get_accounts";
    jsonParser parser(fst.write(request));
    // insert and get dummy method type for testing repository
    LOG_DEBUG << ">>> " << "test method created.";
    Repository::get_instance().add_to_repository("test_method", &test_method);
    Repository::get_instance().get_from_repository("test_method")(parser, postman, &Connection::get_context());

    // setup phase to create method repository
    methods_setup();

    // test get_accounts method
    request_ptr method_ptr = Repository::get_instance().get_from_repository(parser.get_header());

    if(method_ptr) {
        LOG_DEBUG << ">>> " << "get_accounts method found.";
        method_ptr(parser, postman,  &Connection::get_context());
    } else
        LOG_DEBUG << ">>> " << "get accounts method not found.";

    LOG_DEBUG << ">>> " << "process request tests: get_accounts method requested.";
    rdr.parse(processRequest(fst.write(request), &Connection::get_context()), response);
    LOG_DEBUG << ">>> " << "received response: " << fst.write(response);

}


void requester_routine(void* context)
{
    zmqpp::context* cntxt = static_cast<zmqpp::context*>(context);
    zmqpp::socket server(*cntxt, zmqpp::socket_type::rep);

    server.bind("ipc://tests.ipc");

    zmqpp::reactor reactor;

    auto listener = [&server]()
    {
        zmqpp::message message;
        zmqpp::message reply("ACK");

        server.receive(message);
        LOG_DEBUG << ">>> " << "message received: " <<  message.get(0);
        server.send(reply);


    };

    reactor.add(server, listener);

    int counter = 10;

    while(counter--)
    {
        while(reactor.poll()){}
    }
}

void cb_requester_tests(void)
{
    Json::Value dummy_request;
    dummy_request["Method"] = ".get_accounts";
    LOG_DEBUG << ">>> " << "dummy request created. sending to postman.";

    std::thread server(requester_routine, &Connection::get_context());

    zmqpp::socket client(Connection::get_context(), zmqpp::socket_type::req);
    client.connect("ipc://tests.ipc");

    int counter = 10;

    zmqpp::reactor reactor;

    auto listener = [&client]()
    {
        zmqpp::message message;
        client.receive(message);
    };

    reactor.add(client, listener);

    while(counter--)
    {
        zmqpp::message request("HELLO WORLD!");
        client.send(request);
        reactor.poll();
    }

    server.join();
}

void cb_ZC_Keypair_tests()
{
    LOG_DEBUG << "zcash keypair tests begins.";
    ZC_Keypair first_key;
    ZC_Keypair second_key(first_key.serialize());

    assert(first_key.get_recipient_key() == second_key.get_recipient_key());
}

void cb_multi_wallet_tests(void)
{
    ServerEngine s("testdb", true);
}

void start_tests(void)
{
    //cb_multi_wallet_tests();
    //cb_address_tests();
    //cb_db_tests();
    //cb_request_tests();

    //cb_requester_tests();

    //cb_ZC_Keypair_tests();
}