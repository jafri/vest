#include "vest/vest.hpp"

void vest::startvest (
  const extended_asset& deposit,
  const name& vestName,
  const time_point& startTime,
  const time_point& endTime,
  const name& from,
  const name& to,
  const bool& cancellable
) {
  require_auth( from );
  check(is_account(from) && is_account(to), "sending or receiving account does not exist.");

  auto depositAmount = deposit.quantity.amount;
  check(depositAmount > 0, "quantity must be positive.");
  float remainingVest = static_cast<float>(depositAmount);
  extended_symbol depositSymbol = deposit.get_extended_symbol();

  // Substract balance
  subbalance(from, depositSymbol, depositAmount);

  // Find Vest
  auto vests_byfromandname = _vests.get_index<eosio::name("byfromname")>();
  auto vest = vests_byfromandname.find((uint128_t{from.value}<<64) | vestName.value);

  // Vest does not exist
  if (vest == vests_byfromandname.end()) {
    // Vest per second
    auto timespan = static_cast<float>(endTime.sec_since_epoch() - startTime.sec_since_epoch());
    check(timespan > 0, "end time must be after start time.");
    float vestPerSecond = remainingVest / timespan;
    check(vestPerSecond > 0, "vesting per second must be positive");

    // Add vest
    _vests.emplace(from, [&](auto& v) {
        v.id            = _vests.available_primary_key();
        v.vestName      = vestName;
        v.deposit       = deposit;
        v.vestPerSecond = vestPerSecond;
        v.remainingVest = remainingVest;
        v.startTime     = startTime;
        v.endTime       = endTime;
        v.lastVestTime  = startTime;
        v.from          = from;
        v.to            = to;
        v.cancellable   = cancellable;
    });

  // Vest already exists 
  } else {
    auto timeIncrease = static_cast<uint64_t>(depositAmount / vest->vestPerSecond);
    vests_byfromandname.modify(vest, same_payer, [&](auto& v) {
        v.deposit.quantity.amount += depositAmount;
        v.remainingVest           += remainingVest;
        v.endTime                 += eosio::microseconds(timeIncrease * 1000000);
    });
  }
}

void vest::claimvest (
  const name& from,
  const name& vestName
) {
  // Validate vest
  auto vests_byfromandname = _vests.get_index<eosio::name("byfromname")>();
  auto vest = vests_byfromandname.find((uint128_t{from.value}<<64) | vestName.value);
  check(vest != vests_byfromandname.end(), "no vest with ID " + vestName.to_string() + " found.");
  check(has_auth(vest->from) || has_auth(vest->to), "only the sender or receiver of vest can claim vest.");

  // How much to vest
  auto elapsed = current_time_point().sec_since_epoch() - vest->lastVestTime.sec_since_epoch();
  check(elapsed > 0, "vesting has not started yet.");
  auto toVest = static_cast<float>(elapsed) * vest->vestPerSecond;
  check(toVest > 1, "vest quantity must be positive.");
  check(vest->remainingVest > toVest, "cannot vest more than remaining.");

  // Reset to max
  if (toVest > vest->remainingVest || vest->remainingVest - toVest < 1) {
    toVest = vest->remainingVest;
  }

  // Set remaining vest
  vests_byfromandname.modify(vest, same_payer, [&](auto& v) {
      v.remainingVest -= toVest;
      v.lastVestTime   = current_time_point();
  });

  // Send it out
  send(
    vest->deposit.contract,
    vest->to,
    asset(toVest, vest->deposit.quantity.symbol),
    std::string("Vested for vest ID: " + vest->vestName.to_string())
  );

  if (vest->remainingVest == 0) {
    vests_byfromandname.erase(vest);
  }
}

void vest::cancelvest (
  const name& from,
  const name& vestName
) {
  // Get vests
  auto vests_byfromandname = _vests.get_index<eosio::name("byfromname")>();
  auto vest = vests_byfromandname.find((uint128_t{from.value}<<64) | vestName.value);
  check(vest != vests_byfromandname.end(), "no vest with ID " + vestName.to_string() + " found.");
  check(has_auth(vest->from) || has_auth(vest->to), "only the sender or receiver of vest can cancel vest.");
  check(vest->cancellable, "vest is not cancellable.");

  // Pay out
  send(
    vest->deposit.contract,
    vest->from,
    asset(vest->remainingVest, vest->deposit.quantity.symbol),
    std::string("Cancelled Vest ID: " + vest->vestName.to_string())
  );

  // Erase vest
  vests_byfromandname.erase(vest);
}