#ifndef FP_WSTP_LINK_HPP
#define FP_WSTP_LINK_HPP

#include "types.hpp"

#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <variant>
#include <functional>
#include <mutex>

extern "C" {
#include <wstp.h>   // Wolfram Symbolic Transfer Protocol (WSTP)
}


//// WSTP Enviornment Handle ////
class wstp_env {
public:
  wstp_env(WSEnvironmentParameter env_param = nullptr);
  ~wstp_env();
  wstp_env(const wstp_env& other) = delete;                  // copy constructor
  wstp_env& operator=(const wstp_env& other) = delete;       // copy assignment
  wstp_env(wstp_env&& other) = delete;                       // move constructor
  wstp_env& operator=(wstp_env&& other) = delete;            // move assignment

  WSENV borrow_handle() const;  // Member to ensure init
private:
  WSENV handle_;
};


//// WSTP Link Interface ////
class wstp_link {
public:
  // Types:
  using pkt_id_t = int; // WSTP Packet ID
  // Return types:
  using ts_t = std::vector<wsint64>;  // WSTP's 64-bit integer type
  using data_t = std::vector<ts_t>;
  using miss_t = std::pair<ts_t, ts_t>;
  using arg_t = std::variant<wsint64, ts_t, data_t>;
  // Function signatures:
  using fn_t = std::function<arg_t(arg_t)>;
  using sig_t = std::tuple<std::string, std::string>;
  // - Mathematica Definition of Function: Foo[x_Integer]
  // - Mapping of argument name x to transfer type: x, {x}
  using def_t = std::tuple<sig_t, fn_t>;

  // Constants:
  static constexpr char DEFAULT[] = "";
#ifdef __APPLE__
  static constexpr char LAUNCH_KERNEL[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
  static constexpr char CONNECT_TCPIP[] = "-linkmode connect -linkprotocol tcpip -linkname ";
  static constexpr char LISTEN_TCPIP[] = "-linkmode listen -linkprotocol tcpip -linkname ";
#else
  static constexpr char LAUNCH_KERNEL[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#endif

  // Constructors
  wstp_link(std::string args = DEFAULT);
  wstp_link(WSLINK);
  ~wstp_link();

  wstp_link(const wstp_link& other) = delete;                // copy constructor
  wstp_link& operator=(const wstp_link& other) = delete;     // copy assignment
  wstp_link(wstp_link&& other);                     // move constructor
  wstp_link& operator=(wstp_link&& other);          // move assignment

  // Error Handling:
  int error() const;
  std::string get_error() const;
  std::string print_error() const;
  void reset();  // Attempt to recover from error.
  bool log(std::string filename = std::string());
  bool alive() const;

  // Public send/receive interface:
  int ready() const;    // Indicates if data is ready to receive.
  int flush() const;    // Attempt to send any queued messages.
  template< typename Tuple, typename=std::enable_if_t<is_tuple<Tuple>::value> >
    std::vector<Tuple> receive_list();

  // Simple built-in test scenarios:
  int factor_test(int = ((1<<10) * (7*7*7) * 11));
  int factor_test2(int = ((1<<10) * (7*7*7) * 11));

  // Dynamic function handlers:
  static int register_fn(def_t);
  int install();
  void bind();

private:
  // Packet type handlers:
  pkt_id_t receive();
  pkt_id_t receive(pkt_id_t id);
  void receive_worker();

  // Dispatch Table Definitions:
  int decode_call();
  wsint64 close_fn();
  wsint64 wakeup_fn();

  // Library call generation to get/recv data from link:
  template<typename T> T get();
  template<typename T> int put(T);
  template<typename T> int put_function(T, int args = 1);
  template<typename T> int put_return(T, int args = 1);
  template<typename T> int put_symbol(T);
  int put_end();  // Mark end of packet.

  /// Members ///
  WSLINK link_ = nullptr;

  // Global mapping of WSTP function calls:
  static std::vector<def_t> wstp_signatures_;

  // Asyncronous Worker:
  std::vector<fn_t> worker_fTable_;
  std::thread worker_;
  bool worker_stop_ = false;
};


// Generates appropriate get data calls for std::tuple:
template<typename Tuple> Tuple wstp_link::get() {
  return index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      return Tuple(get<typename std::tuple_element<Is, Tuple>::type>()...);
    }
  );
}


// Generates appropriate put data calls for std::tuple:
template<typename Tuple> int wstp_link::put(Tuple t) {
  return index_apply<std::tuple_size<Tuple>{}>(
    [&](auto... Is) {
      int count = 0;
      count += (put(std::get<Is>(t)), ...);
      return count;
    }
  );
}


// Generates appropriate put function calls for std::tuple:
template<typename Tuple> int wstp_link::put_function(Tuple t, int args) {
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
  return count;
}


// Generates appropriate put ReturnPacket for std::tuple:
template<typename Tuple> int wstp_link::put_return(Tuple t, int args) {
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
  return count;
}


// Expects that "List" is at the head of the next packet:
// - Expands based on types of elements in Tuple parameter.
// - Anonomous typename ensures Tuple is indeed a std::tuple
template<typename Tuple, typename>
std::vector<Tuple> wstp_link::receive_list() {
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
