//
// Created by reza on 4/4/18.
//



#include "db.h"

mongocxx::instance database::instance;

database database::instance_db;

//TODO: it's probably hitting the logical error
database::database():database_name("walletDB"),
                     pool(mongocxx::uri()),
                     db()
{
    get_db();
}



database& database::get_instance(std::string db_name)
{

    LOG_WARNING << "getting db instance with name: " << db_name;

    instance_db.set_db(db_name);
    return instance_db;
}

void database::register_db(std::string name)
{
    auto client = pool.acquire();
    auto temp_db = (*client)["DB_REPO"];
    mongocxx::collection coll = temp_db["DB"];

    bsoncxx::builder::basic::document filter;
    mongocxx::options::find options;
    filter.append(bsoncxx::builder::basic::kvp("name", name));
    mongocxx::cursor curser = coll.find(filter.view(), options);

    bool has_element = false;

    for(auto k : curser)
        has_element = true;

    if(!has_element) {
        LOG_WARNING << "registering db with name: " << name;
        bsoncxx::stdx::optional<mongocxx::result::insert_one> result = coll.insert_one(filter.view());
    } else{
        LOG_WARNING << "db already registered: " << name;
    }

}

// TODO: error handling
bool database::drop()
{
    auto client = pool.acquire();
    auto temp_db = (*client)["DB_REPO"];
    mongocxx::collection coll = temp_db["DB"];

    bsoncxx::builder::basic::document filter;
    mongocxx::options::find options;
    mongocxx::cursor curser = coll.find(filter.view(), options);

    for(auto current_db : curser)
    {
        filter.clear();
        Json::Value result;
        Json::Reader rdr;
        rdr.parse(bsoncxx::to_json(current_db), result);
        std::string name = result["name"].asString();
        LOG_WARNING << "droping: " << name;
        auto temp = (*client)[name];
        temp.drop();

        filter.append(bsoncxx::builder::basic::kvp("name", name));
        bsoncxx::stdx::optional<mongocxx::result::delete_result> del_result = coll.delete_many(filter.view());
    }
}

bool database::add_request(Json::Value request, std::string collection_name, std::string user_id)
{
    std::string requestStr;
    try {
        rdr.parse(requestStr, request);

        bsoncxx::document::value doc_value = bsoncxx::from_json(requestStr);
        bsoncxx::document::view view = doc_value.view();

        // TODO: it has to be replaced with something reusable.
        auto client = pool.acquire();
        db = (*client)[database_name];
//        db = get_db();

        mongocxx::collection coll = db[collection_name];

        bsoncxx::stdx::optional<mongocxx::result::insert_one> result =
                coll.insert_one(view);

    }catch (...)
    {
        return false;
    }

    return true;
}

bool database::add_request(std::string request, std::string collection_name, std::string user_id)
{

    try {

        LOG_DEBUG << "database additon process started.";
        LOG_DEBUG << "string to be inserted: " << request;

        bsoncxx::document::value doc_value = bsoncxx::from_json(request);
        bsoncxx::document::view view = doc_value.view();
        LOG_DEBUG << "request string converted to mongodb compatible JSON.";

        // TODO: it has to be replaced with something reusable.
        auto client = pool.acquire();
        db = (*client)[database_name];
       // db = get_db();

        mongocxx::collection coll = db[collection_name];
        LOG_DEBUG << "db successfully connected.";

        bsoncxx::stdx::optional<mongocxx::result::insert_one> result =
                coll.insert_one(view);
        LOG_DEBUG << "requested data inserted to " << coll.name() << " in " << db.name();

    auto cursor = coll.find({});

    for (auto&& doc : cursor) {
        std::cout << bsoncxx::to_json(doc) << std::endl;
    }

    }catch (...)
    {
        Exception("adding to database failed.");
        return false;
    }

    return true;
}


bool database::delete_request(collection result_type,
                    bsoncxx::document::view_or_value filter,
                    int* deleted_number)
{
    try {

        LOG_DEBUG << "database deleting process started.";


        LOG_DEBUG << "request string converted to mongodb compatible JSON.";

        // TODO: it has to be replaced with something reusable.
        auto client = pool.acquire();
        db = (*client)[database_name];
      //  db = get_db();

        mongocxx::collection coll = db.collection(collector[result_type]);
        LOG_DEBUG << "db successfully connected.";

        // deleting process

        bsoncxx::stdx::optional<mongocxx::result::delete_result> result = coll.delete_many(filter);
        *deleted_number = result->deleted_count();

        // #deleting process

    }catch (...)
    {
        Exception("deleting from database failed.");
        return false;
    }

    return true;

}


bool database::update(collection result_type,
                      bsoncxx::document::view_or_value filter,
                      bsoncxx::document::view_or_value update,
                      std::string user_id)
{

    try {
        auto client = pool.acquire();
        mongocxx::database db = (*client)[database_name];
        // db = get_db();
        mongocxx::collection coll = db.collection(collector[result_type]);

        LOG_DEBUG << "db successfully connected for update.";

        bsoncxx::stdx::optional<mongocxx::result::update> result = coll.update_many(filter, update);


        if(result)
            return true;
        else
            return false;

    }catch(...)
    {
        Exception("error occurred in updating database.");
        return false;
    }

}


// TODO: create specific thread for database
//zmq::socket_t* database::s_db_socket(zmq::context_t &context)
//{
//    zmq::socket_t * worker = new zmq::socket_t(context, ZMQ_DEALER);
//
//    //  Set random identity to make tracing easier
//    std::string identity = s_set_id(*worker);
//    worker->connect ("tcp://localhost:8888");
//
//    //  Configure socket to not wait at close time
//    int linger = 0;
//    worker->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
//
//    //  Tell queue we're ready for work
//    LOG_INFO << " (" << identity << ") db_worker ready";
//    s_send(*worker, "READY");
//
//    return worker;
//}
//
//void database::worker_routine(void* context)
//{
//    zmq::context_t* cntxt = static_cast<zmq::context_t*>(context);
//    srandom((unsigned) time(NULL));
//    zmq::socket_t * worker = s_db_socket(*cntxt);
//
//    //  If liveness hits zero, queue is considered disconnected
//    size_t liveness = HEARTBEAT_LIVENESS;
//    size_t interval = INTERVAL_INIT;
//
//    //  Send out heartbeats at regular intervals
//    int64_t heartbeat_at = s_clock() + HEARTBEAT_INTERVAL;
//
//    //queue
//
//
//    while(true) {
//        zmq::pollitem_t items[] = { { (void*)*worker,  0, ZMQ_POLLIN, 0 } };
//        zmq::poll (items, 1, HEARTBEAT_INTERVAL);
//
//        if(items[0].revents & ZMQ_POLLIN) {
//            //  Get message
//            //  - 3-part envelope + content -> request
//            //  - 1-part "HEARTBEAT" -> heartbeat
//
//
//            zmsg msg(*worker);
//
//            if (msg.parts() == 3) {
//                //  Process the request and send reply
//                LOG_INFO << "normal reply - " << msg.body();
//                zmsg reply(msg);
//                reply.body_set(processRequest(msg.body()).c_str());
//                reply.send(*worker);
//                liveness = HEARTBEAT_LIVENESS;
//                //  Do some heavy work
//            }
//            else {
//                if(msg.parts () == 1 && strcmp (msg.body (), "HEARTBEAT") == 0) {
//                    liveness = HEARTBEAT_LIVENESS;
//                }
//                else {
//                    LOG_INFO << "invalid message";
//                    msg.dump ();
//                }
//            }
//            interval = INTERVAL_INIT;
//
//        }
//        else
//        if (--liveness == 0) {
//            LOG_WARNING << "heartbeat failure, can't reach queue";
//            LOG_WARNING << "reconnecting in " << interval << " msec…";
//            s_sleep(interval);
//
//            if(interval < INTERVAL_MAX) {
//                interval *= 2;
//            }
//            delete worker;
//            worker = s_db_socket(*cntxt);
//            liveness = HEARTBEAT_LIVENESS;
//        }
//
//        //  Send heartbeat to queue if it's time
//        if (s_clock() > heartbeat_at) {
//            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
//            //std::cout << "I: (" << identity << ") worker heartbeat" << std::endl;
//            s_send(*worker, "HEARTBEAT");
//        }
//    }
//    delete worker;
//
//    //queue
//}

//std::string database::update_db(std::string)
//{
//    // TODO: indirect access to database through db_worker thread must be implemented.
//    return "result";
//}
