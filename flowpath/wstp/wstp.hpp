#ifndef FP_WSTP_HPP
#define FP_WSTP_HPP

#include "wstp_link.hpp"
#include "wstp_server.hpp"
#include "types.hpp"

#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <variant>
#include <functional>
#include <mutex>

std::vector<wstp_link>::iterator wstp_take_connection(WSLINK wslink);

////////////////////////////////////////
//// Top-level WSTP Library Wrapper ////
class wstp {
 public:
//  wstp();
  template<typename... Args> static auto listen(Args&&...);
  static void wait_for_unlink();

  /// Callback interface
public:
  static std::vector<wstp_link>::iterator take_connection(wstp_link&& link);
  static std::vector<wstp_link>::iterator take_connection(WSLINK wslink);

private:
  /// Members
  static std::mutex mtx_;
  static std::vector<wstp_link> links_;
  static std::vector<wstp_server> servers_;
//  static bool all_unlinked_;
};


template<typename... Args>
auto wstp::listen(Args&&... args) {
  std::lock_guard lck(mtx_);
  auto it = servers_.emplace(servers_.end(), std::forward<Args>(args)...);
  return it->get_interface();
}


#endif // FP_WSTP_HPP
