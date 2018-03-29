#include "messaging_server.h"

#include <string.h>

void worker_t::set_identity(std::string idnt){identity = idnt;}
std::string worker_t::get_identity(void){return identity;}
void worker_t::set_expiry(int64_t expir){expiry = expir;}
int64_t worker_t::get_expiry(void){return expiry;}





zmq::socket_t* worker_t::s_worker_socket(zmq::context_t &context) {

    zmq::socket_t * worker = new zmq::socket_t(context, ZMQ_DEALER);

    //  Set random identity to make tracing easier
    std::string identity = s_set_id(*worker);
    worker->connect ("tcp://localhost:5556");

    //  Configure socket to not wait at close time
    int linger = 0;
    worker->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    //  Tell queue we're ready for work
    LOG_INFO << " (" << identity << ") worker ready";
    s_send(*worker, "READY");

    return worker;
}

void worker_t::worker_routine(void* context)
{
    zmq::context_t* cntxt = static_cast<zmq::context_t*>(context);
    srandom((unsigned) time(NULL));
    zmq::socket_t * worker = s_worker_socket(*cntxt);

    //  If liveness hits zero, queue is considered disconnected
    size_t liveness = HEARTBEAT_LIVENESS;
    size_t interval = INTERVAL_INIT;

    //  Send out heartbeats at regular intervals
    int64_t heartbeat_at = s_clock() + HEARTBEAT_INTERVAL;

    //queue


    while(true) {
        zmq::pollitem_t items[] = { { (void*)*worker,  0, ZMQ_POLLIN, 0 } };
        zmq::poll (items, 1, HEARTBEAT_INTERVAL);

        if(items[0].revents & ZMQ_POLLIN) {
            //  Get message
            //  - 3-part envelope + content -> request
            //  - 1-part "HEARTBEAT" -> heartbeat


            zmsg msg(*worker);

            if (msg.parts() == 3) {
                //  Process the request and send reply
                LOG_INFO << "normal reply - " << msg.body();
                zmsg reply(msg);
                reply.body_set(processRequest(msg.body()).c_str());
                reply.send(*worker);
                liveness = HEARTBEAT_LIVENESS;
                //  Do some heavy work
            }
            else {
                if(msg.parts () == 1 && strcmp (msg.body (), "HEARTBEAT") == 0) {
                    liveness = HEARTBEAT_LIVENESS;
                }
                else {
                    LOG_INFO << "invalid message";
                    msg.dump ();
                }
            }
            interval = INTERVAL_INIT;

        }
        else
        if (--liveness == 0) {
            LOG_WARNING << "heartbeat failure, can't reach queue";
            LOG_WARNING << "reconnecting in " << interval << " msec…";
            s_sleep(interval);

            if(interval < INTERVAL_MAX) {
                interval *= 2;
            }
            delete worker;
            worker = s_worker_socket(*cntxt);
            liveness = HEARTBEAT_LIVENESS;
        }

        //  Send heartbeat to queue if it's time
        if (s_clock() > heartbeat_at) {
            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
            //std::cout << "I: (" << identity << ") worker heartbeat" << std::endl;
            s_send(*worker, "HEARTBEAT");
        }
    }
    delete worker;

    //queue
}




template <class T>
queue<T>::queue(){}

template <class T>
queue<T>::~queue()
{
    original_queue.clear();
}

template <class T>
void queue<T>::_append(std::string identity)
{
    bool found = false;
    for (typename std::vector<T>::iterator it = original_queue.begin(); it < original_queue.end(); it++) {
        if (it->get_identity().compare(identity) == 0) {
            LOG_ERROR << "duplicate worker identity " << identity.c_str() << std::endl;
            found = true;
            break;
        }
    }
    if (!found) {
        T element;
        element.set_identity(identity);
        element.set_expiry(s_clock() + HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS);
        original_queue.push_back(element);
    }

}

template <class T>
void queue<T>::_delete(std::string identity)
{

    for (typename std::vector<T>::iterator it = original_queue.begin(); it < original_queue.end(); it++)
    {
        if (it->get_identity().compare(identity) == 0) {
            original_queue.erase(it);
            break;
        }
    }
}

template <class T>
void queue<T>::_refresh(std::string identity)
{
    bool found = false;
    for (typename std::vector<T>::iterator it = original_queue.begin(); it < original_queue.end(); it++) {
        if (it->get_identity().compare(identity) == 0) {
            it->set_expiry(s_clock () + HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS);
            found = true;
            break;
        }
    }
    if (!found) {
        LOG_ERROR << "worker " << identity << " not ready";
    }
}

//  Pop next available worker off queue, return identity
template <class T>
std::string queue<T>::_dequeue()
{
    assert (original_queue.size());
    std::string identity = original_queue[0].get_identity();
    original_queue.erase(original_queue.begin());
    return identity;
}

template <class T>
void queue<T>::_purge()
{
    int64_t clock = s_clock();
    for (typename std::vector<T>::iterator it = original_queue.begin(); it < original_queue.end(); it++) {
        if (clock > it->get_expiry()) {
            it = original_queue.erase(it)-1;
        }
    }
    //LOG_INFO << "number of available workers: " << original_queue.size();
}

template <class T>
unsigned long queue<T>::size()
{
    return original_queue.size();
}

template <class T>
std::vector<T>& queue<T>::get_queue()
{
    return original_queue;
}

//this line is needed because cpp is stupid!
template class queue<worker_t>;