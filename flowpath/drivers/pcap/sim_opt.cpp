#include "sim_opt.hpp"


ReservationOPT::ReservationOPT(OPT_Time first, OPT_Time last, int references) :
  tDemand_(first), tLast_(last), refCount_(references) {}

ReservationOPT::operator bool() const {
  return valid_ == Action::Reserve;
}

ReservationOPT::OPT_Time ReservationOPT::first() const {
  return tDemand_;
}

ReservationOPT::OPT_Time ReservationOPT::last() const {
  return tLast_;
}

// Extends reservation.  Assumes eligible to reserve in OPT table.
// Returns first t needed to mark range in OPT capacity vector.
ReservationOPT::OPT_Time ReservationOPT::extend(OPT_Time t) {
//  size_t first;
//  if (refCount_ == 1)
//    first = tLast_;      // new reservation confirmed.  [tLast, t]
//  else
//    first =  tLast_ + 1;  // reservation continued from last t.  (tLast, t]
  ++refCount_;
  OPT_Time next = tLast_ + 1;  // always extending reservation (tLast, t]
  tLast_ = t;
  return next;
}
