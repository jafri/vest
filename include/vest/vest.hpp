#pragma once

#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;
using namespace std;

CONTRACT vest : public contract {
  public:
    using contract::contract;

    vest( name receiver, name code, datastream<const char*> ds )
      : contract(receiver, code, ds),
        _vests(receiver, receiver.value),
        _accounts(receiver, receiver.value) {}

    ACTION startvest (const extended_asset& deposit,
                      const std::string& vestName,
                      const float& vestPerSecond,
                      const time_point& startTime,
                      const name& from,
                      const name& to,
                      const bool& cancellable);
    ACTION claimvest (const uint64_t& id,
                      const name& account);
    ACTION cancelvest (const uint64_t& id,
                       const name& account);

    ACTION withdraw (const name& account,
                     const extended_asset& withdrawal);

    ACTION clearall ();
    template <typename T>
    void cleanTable(){
      T db(get_self(), get_self().value);
      auto itr = db.end();
      while(db.begin() != itr){
        itr = db.erase(--itr);
      }
    }

    [[eosio::on_notify("*::transfer")]]
    void transfer( name from,
                   name to,
                   asset quantity,
                   const std::string& memo );

    using transfer_action = action_wrapper<"transfer"_n, &vest::transfer>;

  private:
    TABLE Account {
      name account;
      std::map<extended_symbol, int64_t> balances;
      uint64_t primary_key() const { return account.value; };
    };
    TABLE Vest {
      uint64_t id;
      std::string vestName;
      extended_asset deposit;
      float vestPerSecond;
      float remainingVest;
      time_point startTime;
      time_point lastVestTime;
      name from;
      name to;
      bool cancellable;
      uint64_t primary_key() const { return id; };
    };
    typedef multi_index<"account"_n, Account> account_table;
    typedef multi_index<"vest"_n, Vest> vest_table;

    account_table _accounts;
    vest_table _vests;
};