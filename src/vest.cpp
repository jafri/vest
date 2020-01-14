#include "./vest.hpp"

void vest::clearall () {
  require_auth(get_self());
  cleanTable<vest_table>();
  cleanTable<account_table>();
}

void vest::transfer (name from, name to, asset quantity, const std::string& memo) {
  auto self = get_self();
  auto token_contract = get_first_receiver();
  extended_symbol asset_symbol = extended_asset(quantity, token_contract).get_extended_symbol();

  // Refund stake
  if (from == "eosio.stake"_n) {
    return;
  }

  if (to == self) {
    account_table accounts(self, self.value);
    auto account = accounts.find(from.value);

    // No account
    if (account == accounts.end()) {
      std::map<extended_symbol, int64_t> balanceMap = {
        { asset_symbol, quantity.amount }
      };
      accounts.emplace(self, [&](auto& a) {
        a.account  = from;
        a.balances = balanceMap;
      });
    // Account exists
    } else {
      accounts.modify(account, from, [&](auto& a) {
        a.balances[asset_symbol] += quantity.amount;
      });
    }
  }
}

void vest::startvest (
  const extended_asset& deposit,
  const std::string& vestName,
  const float& vestPerSecond,
  const time_point& startTime,
  const name& from,
  const name& to
) {
  require_auth( from );

  auto self = get_self();
  auto depositAmount = deposit.quantity.amount;
  extended_symbol depositSymbol = deposit.get_extended_symbol();
  check(depositAmount > 0, "quantity must be positive.");
  check(vestPerSecond > 0, "vest per second must be positive.");

  // Substract balance
  account_table accounts(self, self.value);
  auto account = accounts.find(from.value);
  check(account != accounts.end(), "account does not exist");

  accounts.modify(account, from, [&](auto& a) {
    check(a.balances[depositSymbol] >= depositAmount, "not enough deposited");
    a.balances[depositSymbol] -= depositAmount;
  });
  if (account->balances.empty()) {
    accounts.erase(account);
  }

  // Add vest
  vest_table vests(get_self(), get_self().value);
  vests.emplace(get_self(), [&](auto& v) {
      v.id            = vests.available_primary_key();
      v.vestName      = vestName;
      v.deposit       = deposit;
      v.vestPerSecond = vestPerSecond;
      v.remainingVest = static_cast<float>(depositAmount);
      v.startTime     = startTime;
      v.lastVestTime  = startTime;
      v.from          = from;
      v.to            = to;
  });
}

void vest::vestnow (
  const uint64_t& id,
  const name& account
) {
  require_auth( account );

  auto self = get_self();

  // Substract account
  vest_table vests(get_self(), get_self().value);
  auto vest = vests.find(id);
  check(vest != vests.end(), "no vest with ID " + to_string(id) + " found.");
  check(vest->from == account || vest->to == account , "only the sender or receiver of vest can vest.");

  // How much to vest
  auto elapsed = current_time_point().sec_since_epoch() - vest->lastVestTime.sec_since_epoch();
  check(elapsed > 0, "vesting has not started yet.");
  auto toVest = static_cast<float>(elapsed) * vest->vestPerSecond;
  check(toVest > 1, "vest quantity must be positive.");

  // Reset to max
  if (toVest > vest->remainingVest || toVest - vest->remainingVest < 1) {
    toVest = vest->remainingVest;
  }

  vests.modify(vest, account, [&](auto& v) {
      v.remainingVest -= toVest;
      v.lastVestTime   = current_time_point();
  });

  // Send it out
  vest::transfer_action t_action( vest->deposit.contract, {self, "active"_n} );
  t_action.send(self, vest->to, asset(toVest, vest->deposit.quantity.symbol) , std::string("Vested for vest ID: " + to_string(vest->id)));

  if (vest->remainingVest == 0) {
    vests.erase(vest);
  }
}

void vest::withdraw (
  const name& to,
  const extended_asset& withdrawal
) {
  require_auth( to );

  auto self               = get_self();
  auto withdrawalAmount   = withdrawal.quantity.amount;
  auto withdrawalSymbol   = withdrawal.get_extended_symbol();
  auto withdrawalContract = withdrawal.contract;
  check(withdrawalAmount > 0, "quantity must be positive.");

  // Substract account
  account_table accounts(self, self.value);
  auto account = accounts.find(to.value);
  check(account != accounts.end(), "account does not have a balance.");

  accounts.modify(account, to, [&](auto& a) {
    check(a.balances[withdrawalSymbol] >= withdrawalAmount, "not enough deposited");
    a.balances[withdrawalSymbol] -= withdrawalAmount;
  });

  // Withdraw
  vest::transfer_action t_action( withdrawalContract, {self, "active"_n} );
  t_action.send(self, account->account, withdrawal.quantity, std::string("Vest Withdrawal for ID: " + to.to_string()));

  // If no balances left, delete account
  if (account->balances.empty()) {
    accounts.erase(account);
  }
}