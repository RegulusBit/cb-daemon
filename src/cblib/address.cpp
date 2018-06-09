#include "address.h"

CBPaymentAddress::CBPaymentAddress()
{
    sk = libzcash::SpendingKey::random();
    pk = sk.address();

}

CBPaymentAddress::CBPaymentAddress(std::string sk_str)
{
    sk = libzcash::SpendingKey(uint252(uint256S(sk_str)));
    pk = sk.address();
}

CBPaymentAddress::CBPaymentAddress(std::string sk_str, std::string pk_str)
{
    sk = libzcash::SpendingKey(uint252(uint256S(sk_str)));
    pk = libzcash::PaymentAddress(uint256S(pk_str),uint256());
}

Json::Value CBPaymentAddress::serialize()
{
    Json::Value serialized;

    serialized["sk"] = sk.inner().ToString();
    serialized["pk"] = pk.a_pk.ToString();
    return serialized;
}