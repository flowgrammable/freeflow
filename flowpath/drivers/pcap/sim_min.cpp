#include "sim_min.hpp"


Reservation::Reservation() : reservation_min_{}, reservation_time_{} {}

Reservation::Reservation(Time t, MIN_Time col) :
  reservation_min_{col,col}, reservation_time_{t,t} {
  assert(t.tv_sec > 1000000000LL); // simple sanity check that time is real > 2012
}

MIN_Time Reservation::last_col() const {
  return reservation_min_.second;
}

Time Reservation::last_ts() const {
  return reservation_time_.second;
}

bool Reservation::covers(MIN_Time col) const {
//  assert(col >= reservation_min_.first);
  return col <= reservation_min_.second;
}

bool Reservation::strictlyCovers(MIN_Time col) const {
  return col >= reservation_min_.first &&
         col <= reservation_min_.second;
}

MIN_Time Reservation::duration_minTime() const {
  return reservation_min_.second - reservation_min_.first;
}

Time Reservation::duration_time() const {
  timespec tmp = reservation_time_.second - reservation_time_.first;
//  assert(tmp.tv_sec >= 0);
//  assert(tmp.tv_nsec >=0 );
  return tmp;
}

void Reservation::extend(Time t, MIN_Time col) {
  reservation_min_.second = col;
  reservation_time_.second = t;
}
