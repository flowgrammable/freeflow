#ifndef DRIVER_UTIL_CAIDA_HPP
#define DRIVER_UTIL_CAIDA_HPP

#include "port_pcap.hpp"
#include "context.hpp"

#include <map>
#include <string>


template<typename Iterator>
class KeyIterator : public Iterator {
  using Pair = typename Iterator::value_type;
  using Key = typename Pair::first_type;
  using Value = typename Pair::second_type;

public:
    KeyIterator() : Iterator() {};
    KeyIterator(Iterator it_) : Iterator(it_) {};

    Key* operator->() {
      Pair& item = Iterator::operator*();
      return &(item.first);
    }
    Key& operator*() {
      Pair& item = Iterator::operator*();
      return item.first;
    }
};


template<typename Iterator>
class ValueIterator : public Iterator {
  using Pair = typename Iterator::value_type;
  using Key = typename Pair::first_type;
  using Value = typename Pair::second_type;

public:
    ValueIterator() : Iterator() {};
    ValueIterator(Iterator it_) : Iterator(it_) {};

    Value* operator->() {
      Pair& item = Iterator::operator*();
      return &(item.first);
    }
    Value& operator*() {
      Pair& item = Iterator::operator*();
      return item.first;
    }
};



bool timespec_less(const timespec& lhs, const timespec& rhs);
bool timespec_greater(const timespec& lhs, const timespec& rhs);
bool timespec_equal(const timespec& lhs, const timespec& rhs);

struct context_cmp {
  bool operator()(const fp::Context& lhs, const fp::Context& rhs) {
    const timespec& lhs_ts = lhs.packet().timestamp();
    const timespec& rhs_ts = rhs.packet().timestamp();

    return timespec_greater(lhs_ts, rhs_ts);
  }
};

class caidaHandler {
public:
  caidaHandler() = default;
  ~caidaHandler() = default;

private:
  int advance(int id);

public:
  void open_list(int id, std::string listFile);
  void open(int id, std::string file);

  fp::Context recv();
  int rebound(fp::Context* cxt);

private:
  // Next packet in stream from each portID, 'sorted' by arrival timestamp:
  std::priority_queue<fp::Context, std::deque<fp::Context>, context_cmp> next_cxt_;
  // Currently open pcap file hand by portID:
  std::map<int, fp::Port_pcap> pcap_;
  // Next pcap files to open by portID:
  std::map<int, std::queue<std::string>> pcap_files_;

  //// FlowID to stream translation ////
//  std::map<int, uint64_t> pkt_pos_;
};


#endif
