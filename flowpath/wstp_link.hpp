#ifndef FP_WSTP_LINK_HPP
#define FP_WSTP_LINK_HPP

#include <string>
#include <vector>
#include <thread>
#include <iostream>

#include "types.hpp"

extern "C" {
#include <wstp.h>   // Wolfram Symbolic Transfer Protocol (WSTP)
}


class wstp_link {
public:
  using pkt_id_t = int;

  wstp_link();
  wstp_link(int argc, char* argv[]);
  ~wstp_link();

  // Error Handling:
  bool error() const;
  std::string get_error() const;
  std::string print_error() const;
  int reset() const;  // Attempt to recover from error.

  // Public send/receive interface:
  int flush() const;  // Attempt to send any queued messages.
  template< typename Tuple, typename=std::enable_if_t<is_tuple<Tuple>::value> >
    std::vector<Tuple> receive_list() const;

  // Simple built-in test scenarios:
  int factor_test(int = ((1<<10) * (7*7*7) * 11)) const;
  int factor_test2(int = ((1<<10) * (7*7*7) * 11)) const;

private:
  // Packet type handlers:
  pkt_id_t receive() const;
  pkt_id_t receive(pkt_id_t id) const;

  // Library call generation to get/recv data from link:
  template<typename T> T get() const;
  template<typename T> int put(T) const;
  template<typename T> int put_function(T, int args = 1) const;
  int put_end() const;

  void closelink();
  void deinit();

  WSENV env_;
  WSLINK link_;

  // Asyncronous Worker:
  std::thread worker_;
};


// Generates appropriate get data calls for std::tuple:
template<typename Tuple> Tuple wstp_link::get() const {
  return index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      return Tuple(get<typename std::tuple_element<Is, Tuple>::type>()...);
    }
  );
}

// Generates appropriate put data calls for std::tuple:
template<typename Tuple> int wstp_link::put(Tuple t) const {
  return index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      int count = 0;
      count += (put(std::get<Is>(t)), ...);
      return count;
    }
  );
}


// Generates appropriate put function calls for std::tuple:
template<typename Tuple> int wstp_link::put_function(Tuple t, int args) const {
  int count = 0;
  count += put_function("EvaluatePacket");  // assume 1 function for now...
  count += index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      int count = 0;
      count += (put_function(std::get<Is>(t), args), ...);
      return count;
    }
  );
  count += put_end();
  return count; // number of library calls made
}


// Expects that "List" is at the head of the next packet:
// - Expands based on types of elements in Tuple parameter.
// - Anonomous typename ensures Tuple is indeed a std::tuple
template<typename Tuple, typename>
std::vector<Tuple> wstp_link::receive_list() const {
  constexpr int ELEMENTS = static_cast<int>(std::tuple_size<Tuple>::value);
  std::vector<Tuple> v;

  // Skip everything until Return packet type is found:
  receive(RETURNPKT);

  // Expect List returned; len holds list length:
  int listLength;
  if ( !WSTestHead(link_, "List", &listLength) ) {
    std::cerr << "WSTestHead is NULL" << std::endl;
    if (error()) {
      throw print_error();
    }
  }

  // Dynamically read each element in list:
  for (int i = 1; i <= listLength; i++) {  // Mathematica index starts at 1...
    int elements;
    if ( WSTestHead(link_, "List", &elements) ) {
      // Run-time sanity checks:
      if (elements != ELEMENTS) {
        std::cerr << "Error: Expected tuple with " << ELEMENTS
                  << " elements, but received " << elements << ".\n"
                  << i << " of " << listLength << " tuples processed."
                  << std::endl;
        return v;
      }
      v.emplace_back( get<Tuple>() );
    } else {
      std::cerr << "Failed to decode WSTestHead" << std::endl;
      throw print_error();
    }
  }
  return v;
}


#endif // FP_WSTP_LINK_HPP
