#ifndef FP_WSTP_SINGLETON_HPP
#define FP_WSTP_SINGLETON_HPP

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
class wstp_singleton {
 public:
  wstp_singleton();
  ~wstp_singleton();
  template<typename... Args> auto listen(Args&&...);
  void wait_for_unlink();

  /// Callback interface
  static wstp_singleton& getInstance();
  std::vector<wstp_link>::iterator take_connection(wstp_link&& link);
  std::vector<wstp_link>::iterator take_connection(WSLINK wslink);

private:
  /// Members
  std::mutex mtx_;
  std::shared_ptr<wstp_env> env_; // ensures the global wstp_env isn't destroyed too early.
  std::vector<wstp_link> links_;
  std::vector<wstp_server> servers_;

  /// Callback singleton
  static wstp_singleton* singleton_;
};


template<typename... Args>
auto wstp_singleton::listen(Args&&... args) {
  std::lock_guard lck(mtx_);
  auto it = servers_.emplace(servers_.end(), std::forward<Args>(args)...);
  return it->get_interface();
}


#endif // FP_WSTP_SINGLETON_HPP
