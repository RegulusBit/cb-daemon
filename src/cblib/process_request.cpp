//
// Created by reza on 4/17/18.
//



#include "process_request.h"

std::string processRequest(std::string request, void* context)
{
    Json::FastWriter fstr;
    Json::Value response;
    Json::Value temp;
    Json::Reader rdr;
    std::string res;
    std::string wallet_name;

 //   try {


        jsonParser parser(request);
        LOG_DEBUG << "method received: " << parser.get_header() << " with ID: " << parser.get_id();

        response["Method"] = parser.get_header();
        response["Id"] = parser.get_id();

        Postman postman(parser.get_wallet_name()); // it's walletDB by default

        // process methods
        request_ptr reposnse_method = Repository::get_instance().get_from_repository(parser.get_header());

        if (reposnse_method) {
            temp = reposnse_method(parser, postman, context);
            response["result"] = temp["result"];
            response["response"] = temp["response"];
        } else {
            response["result"] = "failed";
            response["response"] = "requested method not found.";
        }


        res = fstr.write(response);

   // }catch(...)
  //  {
   //     LOG_ERROR << "request rejected: " << response["Method"];
  //      response["result"] = "failed";
  //      response["response"] = "error in reading request.";
  //      res = fstr.write(response);
//    }

    LOG_DEBUG << "PROCESS RESPONSE: " << res;
    return res;
}


Repository::Repository():request_type_repository(){}

Repository& Repository::get_instance()
{
    static Repository instance;
    return instance;
}

// adding request type to wallet repository
bool Repository::add_to_repository(std::string name, request_ptr method)
{
    try {
        if(!request_type_repository[name])
            request_type_repository[name] = method;
    }catch(...){
        return false;
    }

    return true;
}

request_ptr Repository::get_from_repository(std::string name)
{
    return request_type_repository[name];
}
