#include "utils.h"


Account::Account()
{
    Balance = 100; // default value of deposited account.
}

Account::Account(__uint64_t balance)
{
    Balance = balance;
}

__uint64_t Account::get_balance()
{
    return Balance;
}


Wallet::Wallet():default_account()
{
    Accounts.push_back(default_account);

}

void Wallet::add_account(Account& adding_account)
{
    Account *temp = &adding_account;
    Accounts.push_back(temp);
}

