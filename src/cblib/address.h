#ifndef ADDRESS_H
#define ADDRESS_H


#include "zcash/Address.hpp"
#include "jsoncpp/json/value.h"
#include "jsoncpp/json/json.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>


class CBPaymentAddress
{
public:
    CBPaymentAddress();
    CBPaymentAddress(std::string);
    CBPaymentAddress(std::string sk_str, std::string pk_str);

    libzcash::SpendingKey get_sk(void)
    {
        return sk;
    }

    libzcash::PaymentAddress get_pk(void)
    {
        return pk;
    }

    Json::Value serialize();

private:
    libzcash::SpendingKey sk;
    libzcash::PaymentAddress pk;
};


#endif