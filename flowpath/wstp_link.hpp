#ifndef FP_WSTP_LINK_HPP
#define FP_WSTP_LINK_HPP

#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <variant>

#include "types.hpp"

extern "C" {
#include <wstp.h>   // Wolfram Symbolic Transfer Protocol (WSTP)
}

////////////////////////////////////
//// RAII WSTP Handle Wrappers: ////
////////////////////////////////////
class wstp_env;
class wstp_server;
class wstp_link;
class wstp;

//// WSTP Enviornment Handle ////
class wstp_env {
public:
  wstp_env(WSEnvironmentParameter env_param = nullptr);
  ~wstp_env();
  wstp_env(const wstp_env& other) = delete;                  // copy constructor
  wstp_env& operator=(const wstp_env& other) = delete;       // copy assignment
  wstp_env(wstp_env&& other) = default;                      // move constructor
  wstp_env& operator=(wstp_env&& other) = default;           // move assignment

  auto borrow_handle() const;  // Member to ensure init
private:
  static int set_up_; // Enforces a single instatiation (~global std::shared_ptr)
  static WSENV handle_;
};


//// WSTP LinkServer Handle ////
class wstp_server {
public:
  wstp_server(const uint16_t port = 0, const std::string ip = std::string());
  ~wstp_server();
  wstp_server(const wstp_server& other) = delete;            // copy constructor
  wstp_server& operator=(const wstp_server& other) = delete; // copy assignment
  wstp_server(wstp_server&& other) = default;                // move constructor
  wstp_server& operator=(wstp_server&& other) = default;     // move assignment

  auto borrow_handle() const;  // Member to ensure init
private:
  static wstp_env env_;
  WSLinkServer handle_;
  std::vector<wstp_link> links_;
};


//// WSTP Link Interface ////
class wstp_link {
public:
  // Types:
  using pkt_id_t = int;
  // Return types:
  using ts_t = std::vector<wsint64>;  // WSTP's 64-bit integer type
  using miss_t = std::pair<ts_t, ts_t>;
  using return_t = std::variant<wsint64, ts_t>;
  // std::function definition:
  using fn_t = std::function<return_t(uint64_t)>;
  using def_t = std::tuple<fn_t, const char*, const char*>;

  // Constants:
#ifdef __APPLE__
  static constexpr char DEFAULT[] = "";
  static constexpr char LAUNCH_KERNEL[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
  static constexpr char CONNECT_TCPIP[] = "-linkmode connect -linkprotocol tcpip -linkname ";
  static constexpr char LISTEN_TCPIP[] = "-linkmode listen -linkprotocol tcpip -linkname ";
#else
  constexpr char LAUNCH_KERNEL[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#endif

  // Constructors
  wstp_link(std::string args = DEFAULT);
  wstp_link(WSLINK);
  ~wstp_link();

  wstp_link(const wstp_link& other) = delete;                // copy constructor
  wstp_link& operator=(const wstp_link& other) = delete;     // copy assignment
  wstp_link(wstp_link&& other) = default;                    // move constructor
  wstp_link& operator=(wstp_link&& other) = default;         // move assignment

  // Error Handling:
  int error() const;
  std::string get_error() const;
  std::string print_error() const;
  void reset();  // Attempt to recover from error.
  bool log(std::string filename = std::string());

  // Public send/receive interface:
  int ready() const;    // Indicates if data is ready to receive.
  int flush() const;    // Attempt to send any queued messages.
  template< typename Tuple, typename=std::enable_if_t<is_tuple<Tuple>::value> >
    std::vector<Tuple> receive_list();

  // Simple built-in test scenarios:
  int factor_test(int = ((1<<10) * (7*7*7) * 11));
  int factor_test2(int = ((1<<10) * (7*7*7) * 11));

  // Dynamic function handlers:
  int install(std::vector<def_t>&);

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

  static wstp_env env_;
  WSLINK link_;

  // Asyncronous Worker:
  std::vector<fn_t> worker_fTable_;
  std::vector<def_t> definitions_;
  std::thread worker_;
  bool worker_stop_;
};


//// Top-level WSTP Orchistration ////
class wstp {
public:
//  wstp();
  template <typename... Args> static void listen(Args&&...);
  static void take_connection(wstp_link&& link);
  static void wait_for_unlink();

private:
  static std::mutex mtx_;
  static std::vector<wstp_link> links_;
  static std::vector<wstp_server> servers_;
  static bool unlink_all_;
};

template<typename... Args>
void wstp::listen(Args&&... args) {
  std::lock_guard lck(mtx_);
  servers_.emplace_back(std::forward<Args>(args)...);
}


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
