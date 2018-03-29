#ifndef MESSAGING_SERVER_H
#define MESSAGING_SERVER_H

#include "plog/Log.h"
#include "utils.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <jsoncpp/json/json.h>
#include "zmsg.hpp"
#ifndef _WIN32
#include <unistd.h>

#else
#include <windows.h>

#define sleep(n)    Sleep(n)
#endif

#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  1000    //  msecs
#define INTERVAL_INIT       1000    //  Initial reconnect
#define INTERVAL_MAX       32000    //  After exponential backoff
#define WORKER_NUM          5


class worker_t
{
public:
    worker_t(){};

    void set_identity(std::string idnt);
    std::string get_identity(void);
    void set_expiry(int64_t expir);
    int64_t get_expiry(void);
    static void worker_routine(void* context);
    static zmq::socket_t *s_worker_socket(zmq::context_t &context);

private:
    std::string identity;
    int64_t expiry;
};



template <class T>
class queue
{
public:
    queue();
    ~queue();
    void _append(std::string identity);
    void _delete(std::string identity);
    void _refresh(std::string identity);
    std::string _dequeue(void);
    void _purge(void);
    unsigned long size(void);
    std::vector<T>& get_queue(void);
private:
    std::vector<T> original_queue;
};


#endif