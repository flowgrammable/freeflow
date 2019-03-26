#ifndef FP_WSTP_LINK_HPP
#define FP_WSTP_LINK_HPP

#include <string>

extern "C" {
#include <wstp.h>   // Wolfram Symbolic Transfer Protocol (WSTP)
}


class wstp_link {
public:
  wstp_link();
  wstp_link(int argc, char* argv[]);
  ~wstp_link();

  // Error Handling:
  bool error() const;
  std::string get_error() const;
  std::string print_error() const;
  int reset() const;

  // Simple built-in test scenarios:
  int factor_test(int = ((1<<10) * (7*7*7) * 11)) const;

private:
  void closelink();
  void deinit();

  WSENV env_;
  WSLINK link_;
};


#endif // FP_WSTP_LINK_HPP
