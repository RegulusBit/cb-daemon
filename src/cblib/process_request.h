//
// Created by reza on 4/17/18.
//

#ifndef TUTSERVER_PROCESS_REQUEST_H
#define TUTSERVER_PROCESS_REQUEST_H


#include <string>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <src/utilities/utils.h>
#include <src/zmq/request.h>


// defining pointer to request processing functions
typedef Json::Value (*request_ptr)(jsonParser parser, Postman& postman, void* context);

// every incoming request would process through this function
std::string processRequest(std::string request, void* context);


// this class would hold all the methods which could be processed.
class Repository
{
public:

    static Repository& get_instance();

    bool add_to_repository(std::string name, request_ptr method);
    request_ptr get_from_repository(std::string name);

    Repository(Repository const&) = delete;
    void operator=(Repository const&) = delete;

private:

    Repository();
    std::unordered_map<std::string, request_ptr> request_type_repository;
};


#endif //TUTSERVER_PROCESS_REQUEST_H
