#pragma once

#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;
using namespace std;

namespace VotingType
{
  const std::string APPROVE = "approve";
  const std::string REJECT  = "reject";
}

CONTRACT vest : public contract {
  public:
    using contract::contract;

    vest( name receiver, name code, datastream<const char*> ds )
      : contract(receiver, code, ds),
        _vests(receiver, receiver.value),
        _accounts(receiver, receiver.value),
        _escrows(receiver, receiver.value) {}

    // Vest
    ACTION startvest (const extended_asset& deposit,
                      const std::string& vestName,
                      const time_point& startTime,
                      const time_point& endTime,
                      const name& from,
                      const name& to,
                      const bool& cancellable);
    ACTION claimvest (const uint64_t& id,
                      const name& account);
    ACTION cancelvest (const uint64_t& id,
                       const name& account);

    // Escrow
    ACTION startescrow (const extended_asset& deposit,
                        const std::string& escrowName,
                        const std::map<name, int>& votesRequired,
                        const int& requiredWeight,
                        const name& from,
                        const name& to);
    ACTION voteescrow ( const uint64_t& id,
                        const name& account,
                        const std::string& approveOrReject );

    // Transfer
    ACTION withdraw (const name& account,
                     const extended_asset& withdrawal);

    [[eosio::on_notify("*::transfer")]]
    void transfer( name from,
                   name to,
                   asset quantity,
                   const std::string& memo );

    using transfer_action = action_wrapper<"transfer"_n, &vest::transfer>;

    // Utils
    ACTION clearall ();
    template <typename T>
    void cleanTable(){
      T db(get_self(), get_self().value);
      auto itr = db.end();
      while(db.begin() != itr){
        itr = db.erase(--itr);
      }
    }

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
      time_point endTime;
      time_point lastVestTime;
      name from;
      name to;
      bool cancellable;
      uint64_t primary_key() const { return id; };
    };
    TABLE Escrow {
      uint64_t id;
      std::string escrowName;
      extended_asset deposit;
      std::map<name, int> votesRequired;
      std::map<name, int> approvalsProvided;
      std::map<name, int> rejectionsProvided;
      int requiredWeight;
      int approvalsWeight;
      int rejectionsWeight;
      name from;
      name to;
      uint64_t primary_key() const { return id; };
    };
    typedef multi_index<"account"_n, Account> account_table;
    typedef multi_index<"vest"_n, Vest> vest_table;
    typedef multi_index<"escrow"_n, Escrow> escrow_table;

    account_table _accounts;
    vest_table _vests;
    escrow_table _escrows;

    void subbalance (
      const name& user,
      const extended_symbol& symbolAndAccount,
      const uint64_t& amount
    );
    void addbalance (
      const name& user,
      const extended_symbol& symbolAndAccount,
      const uint64_t& amount
    );

    void send (
      const name& contract,
      const name& to,
      const asset& quantity,
      const std::string& memo
    );
};