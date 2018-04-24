#include <iostream>
#include "utils.h"
#include "messaging_server.h"
#include <thread>
#include <queue>
#include "account.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <src/cblib/request_methods.h>
#include "cb_tests.h"


int main(int argc , char* argv[])
{
    // using plog library for logging purposes.
    //https://github.com/SergiusTheBest/plog

    plog::init(plog::verbose, "log.out", 1000000, 5);
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);


    LOG_VERBOSE << "The daemon main process starts.";

    pid_t pid; //process ID
    pid_t sid; // session ID

    //TODO: daemon mode
    //EnvironmentSetup(pid, sid);

    // setup wallet
    // wallet would extract the accounts from DB
    methods_setup();
    Wallet user_wallet;

    // TODO: modify test case activation
    LOG_VERBOSE << "development unit tests starts.";
    start_tests();

    zmq::context_t context(1);
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend (context, ZMQ_ROUTER);

    frontend.bind("tcp://*:9155");    //  For clients
    backend.bind ("ipc://workers.ipc");    //  For internal workers


    //  Queue of available workers
    queue<worker_t> workerQueue;

    std::thread worker_thread[WORKER_NUM];

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i] = std::thread(worker_t::worker_routine, &context);

        // random() is not thread safe, this will prevent collision in using s_set_id() in threads.
        random();
        LOG_INFO << "The" << i << "th worker thread created.";
    }

    //  Send out heartbeats at regular intervals
    int64_t heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;

    while(true)
    {
        zmq::pollitem_t items[] = {
                {(void*)backend,  0, ZMQ_POLLIN, 0},
                {(void*)frontend, 0, ZMQ_POLLIN, 0}
        };
        //  Poll frontend only if we have available workers
        if (workerQueue.size()) {
            zmq::poll(items, 2, HEARTBEAT_INTERVAL);
        } else {
            LOG_INFO << "The worker queue is empty.";
            zmq::poll(items, 1, HEARTBEAT_INTERVAL);
        }

        // worker

        //  Handle worker activity on backend
        if (items[0].revents & ZMQ_POLLIN) {
            zmsg msg(backend);

            //the unwrap method somehow drops two frames(parts)
            //from message which is not correct
            //so the pop method hard coded instead.
            //std::string identity(msg.unwrap());
            std::string identity = (char*)msg.pop_front().c_str();

            //  Return reply to client if it's not a control message
            if (msg.parts () == 1) {
                if (strcmp (msg.address (), "READY") == 0) {
                    workerQueue._delete(identity);
                    workerQueue._append(identity);
                }
                else
                {
                    if (strcmp(msg.address(), "HEARTBEAT") == 0) {
                        workerQueue._refresh(identity);
                    } else{
                        LOG_WARNING << "invalid message from " << identity;
                        msg.dump ();
                    }
                }
            }
            else{
                LOG_INFO << "Message received from client " << identity << " and sent to worker";
                msg.send(frontend);
                workerQueue._append(identity);
            }
        }

        // worker

        //client

        if (items [1].revents & ZMQ_POLLIN) {
            //  Now get next client request, route to next worker
            zmsg msg(frontend);
            std::string identity = std::string(workerQueue._dequeue());
            msg.push_front((char*)identity.c_str());
            msg.send(backend);
        }

        //client

        //  Send heartbeats to idle workers if it's time
        if (s_clock () > heartbeat_at) {
            for (std::vector<worker_t>::iterator it = workerQueue.get_queue().begin();
                 it < workerQueue.get_queue().end(); it++) {
                zmsg msg ("HEARTBEAT");
                msg.wrap (it->get_identity().c_str(), NULL);
                msg.send (backend);
            }
            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
        }
        workerQueue._purge();
    }

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i].join();
    }
    LOG_INFO << "daemon main process finished.";
    return 0;
}




//test

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
//}