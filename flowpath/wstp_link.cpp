#include "wstp_link.hpp"

#include <iostream>
#include <sstream>

#include <cstdlib>

#ifdef __APPLE__
constexpr char ARGS[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#else
constexpr char ARGS[] = "-linkmode launch -linkname /Applications/Mathematica.app/Contents/MacOS/WolframKernel -wstp";
#endif


wstp_link::wstp_link() : env_(nullptr), link_(nullptr) {
  env_ = WSInitialize(nullptr);
  if (!env_)
    throw std::string("Failed WSInitialize.");

  int err = 0;
  link_ = WSOpenString(env_, ARGS, &err);
  if(!link_ || err != WSEOK) {
    std::cerr << "WSOpenArgcArgv Error: " << err << std::endl;
    throw std::string("Failed WSOpenArgv.");
  }
}

wstp_link::wstp_link(int argc, char* argv[]) : env_(nullptr), link_(nullptr) {
  env_ = WSInitialize(nullptr);
  if (!env_)
    throw std::string("Failed WSInitialize.");

  int err = 0;
  link_ = WSOpenArgcArgv(env_, argc, argv, &err);
  if(!link_ || err != WSEOK) {
    std::cerr << "WSOpenArgcArgv Error: " << err << std::endl;
    throw std::string("Failed WSOpenArgv.");
  }
}

wstp_link::~wstp_link() {
  deinit();
  closelink();
}

void wstp_link::closelink(void) {
  if (link_)
    WSClose(link_);
  link_ = nullptr;
}

void wstp_link::deinit(void) {
  if (env_)
    WSDeinitialize(env_);
  env_ = nullptr;
}

int wstp_link::reset() const {
  if ( WSClearError(link_) == 0) {
    throw print_error();
  }
  return WSNewPacket(link_);
}

bool wstp_link::error() const {
  return WSError(link_);
}

std::string wstp_link::get_error() const {
  if ( int code = WSError(link_) ) {
    const char* err = WSErrorMessage(link_);
    std::stringstream ss;
    ss << "wstp_link error " << code << ": " << err;
    WSReleaseErrorMessage(link_, err);
    return ss.str();
  } else {
    return std::string();
  }
}

std::string wstp_link::print_error() const {
  if (error()) {
    std::string err = get_error();
    std::cerr << err << std::endl;
    return err;
  }
  return std::string();
}

int wstp_link::factor_test(int n) const {
  std::cout << "Factors of " << n << ":\n";

  // Send actual Mathematica command:
  WSPutFunction(link_, "EvaluatePacket", 1L);
  WSPutFunction(link_, "FactorInteger", 1L);
  WSPutInteger(link_, n);
  WSEndPacket(link_);

  int pkt = 0;
  // note: fixed weird syntax...  ?was checking old pkt value?
  while( (pkt = WSNextPacket(link_)) != RETURNPKT) {
    WSNewPacket(link_);
    if (error()) {
      throw print_error();
    }
  }

  // Expect list returned; len holds list length:
  int len = 0;
  if ( !WSTestHead(link_, "List", &len) ) {
    std::cerr << "WSTestHead is NULL" << std::endl;
    if (error()) {
      throw print_error();
    }
  }

  // For each element in list,
  for (int k = 1; k <= len; k++) {
    int lenp = 0, prime = 0, expt = 0;
    if (WSTestHead(link_, "List", &lenp)
        &&  lenp == 2
        &&  WSGetInteger(link_, &prime)
        &&  WSGetInteger(link_, &expt) )
    {
      std::cout << prime << " ^ " << expt << std::endl;
    } else {
      std::cerr << "Failed to decode WSTestHead" << std::endl;
      throw print_error();
    }
  }

//  WSPutFunction(link_, "Exit", 0);
  return EXIT_SUCCESS;
}
