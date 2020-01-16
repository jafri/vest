#include "vest/vest.hpp"

void vest::startescrow (
    const extended_asset& deposit,
    const std::string& escrowName,
    const std::map<name, int>& votesRequired,
    const int& requiredWeight,
    const name& from,
    const name& to
) {
  require_auth( from );

  auto depositAmount = deposit.quantity.amount;
  extended_symbol depositSymbol = deposit.get_extended_symbol();
  check(depositAmount > 0, "quantity must be positive.");
  check(!votesRequired.empty(), "atleast one voter must be provided");
  check(requiredWeight > 0, "atleast a weight of 1 is required");

  // Check that approvers are all accounts
  for( auto const& [voter, weight] : votesRequired ) {
    check(is_account(voter), voter.to_string() + " is not an account");
  }

  // Substract balance
  subbalance(from, depositSymbol, depositAmount);

  // Add escrow
  _escrows.emplace(get_self(), [&](auto& e) {
      e.id             = _escrows.available_primary_key();
      e.deposit        = deposit;
      e.escrowName     = escrowName;
      e.votesRequired  = votesRequired;
      e.requiredWeight = requiredWeight;
      e.from           = from;
      e.to             = to;
  });
}

void vest::voteescrow (
  const uint64_t& id,
  const name& account,
  const std::string& approveOrReject
) {
  require_auth( account );

  // Find Escrow
  auto escrow = _escrows.find(id);
  check(escrow != _escrows.end(), "no escrow with ID " + to_string(id) + " found.");
  check(escrow->votesRequired.find(account) != escrow->votesRequired.end(), account.to_string() + " is not an approver for escrow ID " + to_string(id));
  check(escrow->approvalsProvided.find(account) == escrow->approvalsProvided.end(), account.to_string() + " has already approved escrow ID " + to_string(id));
  check(escrow->rejectionsProvided.find(account) == escrow->rejectionsProvided.end(), account.to_string() + " has already rejected escrow ID " + to_string(id));
  check(approveOrReject == VotingType::APPROVE || approveOrReject == VotingType::REJECT, "must approve or reject escrow");

  // Add approval
  _escrows.modify(escrow, same_payer, [&](auto& e) {
    if (approveOrReject == VotingType::APPROVE) {
        e.approvalsProvided[account] = e.votesRequired[account];
        e.approvalsWeight += e.votesRequired[account];
    } else {
        e.rejectionsProvided[account] = e.votesRequired[account];
        e.rejectionsWeight += e.votesRequired[account];
    }
  });

  // Met threshold for approvals
  if (escrow->approvalsWeight >= escrow->requiredWeight) {
    send(
        escrow->deposit.contract,
        escrow->to,
        escrow->deposit.quantity,
        std::string("Escrow complete for ID: " + to_string(escrow->id))
    );
    _escrows.erase(escrow);
  // Met thresholds for rejections
  } else if (escrow->rejectionsWeight >= escrow->requiredWeight) {
    send(
        escrow->deposit.contract,
        escrow->from,
        escrow->deposit.quantity,
        std::string("Escrow incomplete for ID: " + to_string(escrow->id))
    );
    _escrows.erase(escrow);
  }
}