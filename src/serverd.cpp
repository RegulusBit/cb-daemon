#include <iostream>
#include <string>
#include "utils.h"
#include "logger.h"
#include "messaging_server.h"
#include <thread>
#include <queue>
#include "zhelpers.hpp"

using namespace std;

static const int WORKER_NUM = 10;

void log(std::string strng)
{
    std::cout << strng << std::endl;
}

src::logger_mt& lg = init();

void worker_routine(void* cntxt)
{
    log("worker created!");
    zmq::context_t *cntxtPtr = (zmq::context_t*)cntxt;
    zmq::socket_t socket(*cntxtPtr, ZMQ_REQ);
    socket.connect("ipc://workers.ipc");

    int count = 0;
    s_send(socket, "Ready!");

    while(true)
    {
        std::string address = s_recv(socket);

        {
            assert(s_recv(socket).size() == 0);
        }

        std::string message = s_recv(socket);

        if(message.find("Kill!") == 0)
        {
            s_sendmore(socket, address);
            s_sendmore(socket, "");
            s_send(socket, "OK!killing!");
            std::cout << "Completed tasks: " << count << std::endl;
            break;
        }
        message.append(" Received. thanks for your consideration!");

        s_sendmore(socket, address);
        s_sendmore(socket, "");
        s_send(socket, message);
        count++;
    }
    log("worker destroyed!");
}

void client_routine()
{
    log("client created!");
    zmq::context_t cntxt(1);
    zmq::socket_t socket(cntxt, ZMQ_REQ);

    s_set_id(socket);
    socket.connect("tcp://localhost:5990");

    uint64_t deadline = s_clock() + 10000;

    while(true)
    {

        s_send(socket, "Hello");
        std::string reply = s_recv(socket);
        //log(reply);

        if(s_clock() > deadline)
            {
                s_send(socket, "Kill!");
                std::string reply = s_recv(socket);
                log(reply);
                break;
            }
    }
    log("client destroyed!");
}

int main(int argc , char* argv[])
{
    std::thread twrk[WORKER_NUM];
    std::thread tcli[WORKER_NUM];

    int client_nbr = WORKER_NUM;

    zmq::context_t context(1);

    BOOST_LOG(lg) << "Server starting point. initialization...";
    log("Server initialization...");

    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend(context, ZMQ_ROUTER);

    std::queue<std::string> worker_queue;

    backend.bind("ipc://workers.ipc");
    frontend.bind("tcp://*:5990");

    zmq::pollitem_t items[]=
            {
                    {(void*)backend, 0, ZMQ_POLLIN, 0},
                    {(void*)frontend, 0, ZMQ_POLLIN, 0}
            };

    for(int i=0 ; i < WORKER_NUM ; i++)
    {
        tcli[i] = std::thread(client_routine);
        twrk[i] = std::thread(worker_routine, (void*)&context);
    }

    while(true)
    {
        //important!
        if (worker_queue.size())
            zmq::poll(&items[0], 2, -1);
        else
            zmq::poll(&items[0], 1, -1);


        if(items[0].revents && ZMQ_POLLIN)
        {
            worker_queue.push(s_recv(backend));
            assert(s_recv(backend).size() == 0);

            std::string client_address = s_recv(backend);
            if(client_address.compare("Ready!") != 0)
            {
                assert(s_recv(backend).size() == 0);
                std::string message = s_recv(backend);
                s_sendmore(frontend, client_address);
                s_sendmore(frontend, "");
                s_send(frontend, message);


            }

        }

        if(items[1].revents && ZMQ_POLLIN)
        {
            std::string client_address = s_recv(frontend);
            {
                std::string empty = s_recv(frontend);
                assert(empty.size() == 0);
            }

            std::string request = s_recv(frontend);

            std::string worker_addr = worker_queue.front();
            worker_queue.pop();

            s_sendmore(backend, worker_addr);
            s_sendmore(backend, "");
            s_sendmore(backend, client_address);
            s_sendmore(backend, "");
            s_send(backend, request);

        }

    }

    for(int i=0 ; i < WORKER_NUM ; i++)
    {
        tcli[i].join();
        twrk[i].join();
    }


    return 0;
}
