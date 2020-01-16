#include "vest/vest.hpp"

void vest::clearall () {
  require_auth(get_self());
  cleanTable<vest_table>();
  cleanTable<account_table>();
  cleanTable<escrow_table>();
}