#ifndef FP_WSTP_SERVER_HPP
#define FP_WSTP_SERVER_HPP

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

////////////////////////////////////////
//// WSTP LinkServer Handle Wrapper ////
class wstp_server {
public:
  wstp_server(const uint16_t port = 0, const std::string ip = std::string());
  ~wstp_server();

  wstp_server(const wstp_server& other) = delete;            // copy constructor
  wstp_server& operator=(const wstp_server& other) = delete; // copy assignment
  wstp_server(wstp_server&& other);                 // move constructor
  wstp_server& operator=(wstp_server&& other);      // move assignment

  auto borrow_handle() const;  // Member to ensure init

private:
  /// Members ///
  WSLinkServer server_;
};

#endif // FP_WSTP_SERVER_HPP
