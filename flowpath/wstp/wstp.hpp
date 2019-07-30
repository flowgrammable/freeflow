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


//// Top-level WSTP Orchistration ////

std::vector<std::unique_ptr<wstp_link>>::iterator
wstp_take_connection(WSLINK wslink);


class wstp {
 public:
//  wstp();
  template<typename... Args> static auto listen(Args&&...);
  static void wait_for_unlink();

private:
  static std::mutex mtx_;
  static std::vector<std::unique_ptr<wstp_link>> links_;
  static std::vector<std::unique_ptr<wstp_server>> servers_;
  static bool unlink_all_;

  // Callback interface
public:
  //  static void take_connection(wstp_link&& link);
  static std::vector<std::unique_ptr<wstp_link>>::iterator take_connection(WSLINK wslink);
};


template<typename... Args>
auto wstp::listen(Args&&... args) {
  std::lock_guard lck(mtx_);
  auto ptr = std::make_unique<wstp_server>( std::forward<Args>(args)... );
  return servers_.emplace(servers_.end(), std::move(ptr));
}


#endif // FP_WSTP_HPP
