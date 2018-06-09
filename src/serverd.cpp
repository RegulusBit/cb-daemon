#include <iostream>
#include "utils.h"
#include "messaging_server.h"
#include <thread>
#include <queue>
#include "account.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <src/cblib/request_methods.h>
#include <src/zmq/request.h>
#include "cb_tests.h"

int main(int argc , char* argv[])
{

    // using plog library for logging purposes.
    //https://github.com/SergiusTheBest/plog

    plog::init(plog::verbose, "log.out", 1000000, 5);
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);


    LOG_VERBOSE << "The daemon main process starts.";
    //database::get_instance(DEFAULT_DB).drop();

    pid_t pid; //process ID
    pid_t sid; // session ID

    //TODO: daemon mode
    //EnvironmentSetup(pid, sid);

    // setup wallet
    // wallet would extract the accounts from DB
    methods_setup();



    // TODO: modify test case activation
    LOG_VERBOSE << "development unit tests starts.";
    start_tests();

    Connection::get_context();


    zmqpp::socket frontend(Connection::get_context(), zmqpp::socket_type::router);
    zmqpp::socket backend(Connection::get_context(), zmqpp::socket_type::router);




    frontend.bind("tcp://*:9155");    //  For clients
    backend.bind ("ipc://workers.ipc");    //  For internal workers


    //  Queue of available workers
    queue<worker_t> workerQueue;

    std::thread worker_thread[WORKER_NUM];

    for(int i=0; i < WORKER_NUM; i++)
    {
        //TODO: std::ref
        worker_thread[i] = std::thread(worker_t::worker_routine, &Connection::get_context());
        LOG_INFO << "The " << i << "th worker thread created.";
    }


    //  Send out heartbeats at regular intervals
    int64_t heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;


    zmqpp::reactor reactor;
    zmqpp::poller& poller = reactor.get_poller();

    auto socket_listener = [&poller, &backend, &frontend, &workerQueue, &heartbeat_at](){


        // worker

        //  Handle worker activity on backend
        if (poller.has_input(backend)) {
            zmqpp::message msg;
            backend.receive(msg);

            //the unwrap method somehow drops two frames(parts)
            //from message which is not correct
            //so the pop method hard coded instead.
            std::string identity = msg.get(0);



            //  Return reply to client if it's not a control message
            if (msg.parts () == 2) {
                if (strcmp (msg.get(1).c_str(), "READY") == 0) {
                    workerQueue._delete(identity);
                    workerQueue._append(identity);
                }
                else
                {
                    if (strcmp(msg.get(1).c_str(), "HEARTBEAT") == 0) {
                        workerQueue._refresh(identity);
                    } else{
                        LOG_WARNING << "invalid message from " << identity;
                    }
                }
            }
            else{
                LOG_INFO << "Message received from worker " << identity << " and sent to client.";
                msg.remove(0);
                frontend.send(msg);
                workerQueue._append(identity);
            }
        }

        // worker

        //client

        if (poller.has_input(frontend) && workerQueue.size()) {
            //  Now get next client request, route to next worker
            zmqpp::message msg;
            frontend.receive(msg);
            std::string identity = std::string(workerQueue._dequeue());
            msg.push_front((char*)identity.c_str());
            backend.send(msg);
        }

        //client

        //  Send heartbeats to idle workers if it's time
        if (s_clock () > heartbeat_at) {
            for (std::vector<worker_t>::iterator it = workerQueue.get_queue().begin();
                 it < workerQueue.get_queue().end(); it++) {
                zmqpp::message msg("HEARTBEAT");
                msg.push_front(it->get_identity().c_str());
                backend.send(msg);
            }
            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
        }
        workerQueue._purge();

    };


    reactor.add( frontend, socket_listener );
    reactor.add( backend, socket_listener );

    while(reactor.poll()) {}

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i].join();

    }

    LOG_INFO << "daemon main process finished.";
    return 0;
}




//test
//
//#include <jsoncpp/json/json.h>
//#include "cb_tests.h"
//#include <iostream>
//
//
//int main()
//{
//
//    //using plog library for logging purposes.
//    //https://github.com/SergiusTheBest/plog
//
//    plog::init(plog::verbose, "log.out", 1000000, 5);
//    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
//    plog::init(plog::verbose, &consoleAppender);
//
//    start_tests();
//
//    return 0;
//}