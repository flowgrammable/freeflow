#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <string>

#include <boost/optional.hpp>

using opt_string_ = boost::optional<std::string>;

struct global_config {
  // Trace File Generation:
  opt_string_ logFilename;
  opt_string_ decodeLogFilename;
  opt_string_ flowsFilename;
  opt_string_ statsFilename;
  opt_string_ traceFilename;
  opt_string_ traceTCPFilename;
  opt_string_ traceUDPFilename;
  opt_string_ traceOtherFilename;
  opt_string_ traceScansFilename;

  // Cache Simulation Configs:


  // WSTP Configs:
//  opt_port_ wstp_port;
};

#endif // GLOBAL_CONFIG_H
