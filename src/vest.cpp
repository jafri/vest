#include "vest/vest.hpp"

void vest::startvest (
  const extended_asset& deposit,
  const std::string& vestName,
  const time_point& startTime,
  const time_point& endTime,
  const name& from,
  const name& to,
  const bool& cancellable
) {
  require_auth( from );

  auto depositAmount = deposit.quantity.amount;
  extended_symbol depositSymbol = deposit.get_extended_symbol();
  check(depositAmount > 0, "quantity must be positive.");

  // Substract balance
  subbalance(from, depositSymbol, depositAmount);

  // Vest per second
  float remainingVest = static_cast<float>(depositAmount);
  auto timespan = static_cast<float>(endTime.sec_since_epoch() - startTime.sec_since_epoch());
  check(timespan > 0, "end time must be after start time.");
  float vestPerSecond = remainingVest / timespan;
  check(vestPerSecond > 0, "vesting per second must be positive.");

  // Add vest
  _vests.emplace(get_self(), [&](auto& v) {
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
}

void vest::claimvest (
  const uint64_t& id,
  const name& account
) {
  require_auth( account );

  // Substract account
  auto vest = _vests.find(id);
  check(vest != _vests.end(), "no vest with ID " + to_string(id) + " found.");
  check(vest->from == account || vest->to == account , "only the sender or receiver of vest can vest.");

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
  _vests.modify(vest, same_payer, [&](auto& v) {
      v.remainingVest -= toVest;
      v.lastVestTime   = current_time_point();
  });

  // Send it out
  send(
    vest->deposit.contract,
    vest->to,
    asset(toVest, vest->deposit.quantity.symbol),
    std::string("Vested for vest ID: " + to_string(vest->id))
  );

  if (vest->remainingVest == 0) {
    _vests.erase(vest);
  }
}


void vest::cancelvest (
  const uint64_t& id,
  const name& account
) {
  require_auth( account );

  // Get vests
  auto vest = _vests.find(id);
  check(vest != _vests.end(), "no vest with ID " + to_string(id) + " found.");
  check(vest->from == account || vest->to == account, "only the sender or receiver of vest can cancel vest.");
  check(vest->cancellable, "vest is not cancellable.");

  // Pay out
  send(
    vest->deposit.contract,
    vest->from,
    asset(vest->remainingVest, vest->deposit.quantity.symbol),
    std::string("Cancelled Vest ID: " + to_string(vest->id))
  );

  // Erase vest
  _vests.erase(vest);
}