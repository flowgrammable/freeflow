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
  // Types:
  using pkt_id_t = int;
  using ts_t = std::vector<wsint64>;
  using fn_t = std::function<ts_t()>;

  // Constants:
#ifdef __APPLE__
  static constexpr char LAUNCH_KERNEL[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
  static constexpr char CONNECT_TCPIP[] = "-linkmode connect -linkprotocol tcpip -linkname ";
  static constexpr char LISTEN_TCPIP[] = "-linkmode listen -linkprotocol tcpip -linkname ";
#else
  constexpr char LAUNCH_KERNEL[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#endif

  // Constructors
  wstp_link(std::string args = LAUNCH_KERNEL);
  wstp_link(int argc, const char* argv[]);
  ~wstp_link();

  // Error Handling:
  bool error() const;
  std::string get_error() const;
  std::string print_error() const;
  int reset() const;  // Attempt to recover from error.
  bool log(std::string filename) const;

  // Public send/receive interface:
  int ready() const;    // Indicates if data is ready to receive.
  int flush() const;    // Attempt to send any queued messages.
  template< typename Tuple, typename=std::enable_if_t<is_tuple<Tuple>::value> >
    std::vector<Tuple> receive_list() const;

  // Simple built-in test scenarios:
  int factor_test(int = ((1<<10) * (7*7*7) * 11)) const;
  int factor_test2(int = ((1<<10) * (7*7*7) * 11)) const;

  void wait();

//private:
  // Packet type handlers:
  pkt_id_t receive() const;
  pkt_id_t receive(pkt_id_t id) const;
  void receive_worker() const;

  // Package / External Definitions:
  int install(fn_t func);

  // Library call generation to get/recv data from link:
  template<typename T> T get() const;
  template<typename T> int put(T) const;
  template<typename T> int put_function(T, int args = 1) const;
  template<typename T> int put_return(T, int args = 1) const;
  template<typename T> int put_symbol(T) const;
  int put_end() const;  // Mark end of packet.

  void closelink();
  void deinit();

  WSENV env_;
  WSLINK link_;

  // Asyncronous Worker:
  std::vector<fn_t> worker_fTable_;
  std::thread worker_;
  bool worker_stop_;
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
  count += put_function("EvaluatePacket", 1);  // assume 1 function for now...
  count += index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      int count = 0;
      count += (put_function(std::get<Is>(t), args), ...);
      return count;
    }
  );
  count += put_end();
  flush();
  return count; // number of library calls made
}


// Generates appropriate put ReturnPacket for std::tuple:
template<typename Tuple> int wstp_link::put_return(Tuple t, int args) const {
  int count = 0;
  count += put_function("ReturnPacket", args);  // assume 1 function for now...
  count += index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      int count = 0;
      count += (put(std::get<Is>(t)), ...);
      return count;
    }
  );
  count += put_end();
  flush();
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
