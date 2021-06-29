#include "sim_min.hpp"


Reservation::Reservation() : reservation_min_{}, reservation_time_{} {}

Reservation::Reservation(Time t, MIN_Time col, MIN_Time acc) :
  reservation_min_{col,col}, reservation_time_{t,t}, reservation_access_{acc,acc} {
  assert(t.tv_sec > 1000000000LL); // simple sanity check that time is real > 2012
}

MIN_Time Reservation::last_col() const {
  return reservation_min_.second;
}

MIN_Time Reservation::last_acc() const {
  return reservation_access_.second;
}

Time Reservation::last_ts() const {
  return reservation_time_.second;
}

uint64_t Reservation::hits() const {
  return hits_;
}

bool Reservation::covers(MIN_Time col) const {
//  assert(col >= reservation_min_.first);
  return col <= reservation_min_.second;
}

bool Reservation::strictlyCovers(MIN_Time col) const {
  return col >= reservation_min_.first &&
         col <= reservation_min_.second;
}

MIN_Time Reservation::duration_minTime(MIN_Time t) const {
  if (t == MIN_Time{}) {
    t = reservation_min_.second;
  }
  return t - reservation_min_.first;
}

MIN_Time Reservation::duration_access(MIN_Time t) const {
  if (t == MIN_Time{}) {
    t = reservation_access_.second;
  }
  return t - reservation_access_.first;
}

Time Reservation::duration_time(Time t) const {
  if (t == Time{}) {
    t = reservation_time_.second;
  }
  return t - reservation_time_.first;
}

void Reservation::extend(Time t, MIN_Time col, MIN_Time acc) {
  reservation_min_.second = col;
  reservation_access_.second = acc;
  reservation_time_.second = t;
  ++hits_;
}
