#include "vest/vest.hpp"

void vest::send (
  const name& contract,
  const name& to,
  const asset& quantity,
  const std::string& memo
) {
  vest::transfer_action t_action( contract, {get_self(), "active"_n} );
  t_action.send(get_self(), to, quantity, memo);
}

void vest::transfer (name from, name to, asset quantity, const std::string& memo) {
  auto token_contract = get_first_receiver();
  extended_symbol asset_symbol = extended_asset(quantity, token_contract).get_extended_symbol();

  // Refund stake
  if (from == "eosio.stake"_n) {
    return;
  }

  // Receive balance
  if (to == get_self()) {
    addbalance(from, asset_symbol, quantity.amount);
  }
}

void vest::withdraw (
  const name& to,
  const extended_asset& withdrawal
) {
  require_auth( to );

  auto withdrawalAmount   = withdrawal.quantity.amount;
  auto withdrawalSymbol   = withdrawal.get_extended_symbol();
  auto withdrawalContract = withdrawal.contract;
  check(withdrawalAmount > 0, "quantity must be positive.");

  // Substract account
  subbalance(to, withdrawalSymbol, withdrawalAmount);

  // Withdraw
  send(
    withdrawalContract,
    to,
    withdrawal.quantity,
    std::string("Withdraw balance for: " + to.to_string())
  );
}

void vest::addbalance (
  const name& user,
  const extended_symbol& symbolAndAccount,
  const uint64_t& amount
) { 
  auto account = _accounts.find(user.value);

  // No account
  if (account == _accounts.end()) {
    std::map<extended_symbol, int64_t> balanceMap = {
      { symbolAndAccount, amount }
    };
    _accounts.emplace(get_self(), [&](auto& a) {
      a.account  = user;
      a.balances = balanceMap;
    });
  // Account exists
  } else {
    _accounts.modify(account, same_payer, [&](auto& a) {
      a.balances[symbolAndAccount] += amount;
    });
  }
}

void vest::subbalance (
  const name& user,
  const extended_symbol& symbolAndAccount,
  const uint64_t& amount
) {
  auto account = _accounts.find(user.value);
  check(account != _accounts.end(), "account does not have a balance.");

  _accounts.modify(account, same_payer, [&](auto& a) {
    check(a.balances[symbolAndAccount] >= amount, "not enough deposited");

    a.balances[symbolAndAccount] -= amount;

    // Erase balance if 0
    if (a.balances[symbolAndAccount] == 0) {
      a.balances.erase(symbolAndAccount);
    }
  });

  // If no balances left, delete account
  if (account->balances.empty()) {
    _accounts.erase(account);
  }
}