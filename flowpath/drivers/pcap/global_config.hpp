#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <string>
#include<filesystem>

#include <boost/optional.hpp>

namespace fs = std::filesystem;

using opt_string = boost::optional<std::string>;
//using opt_bool = boost::optional<bool>;

struct global_config {
  fs::path outputDir;
  std::vector<std::string> configFiles;
  std::string simStartTime_str;
  std::string simRunName_str;

  // Trace File Generation:
  opt_string logFilename;
  opt_string decodeLogFilename;
  opt_string flowsFilename;
  opt_string statsFilename;
  opt_string traceFilename;
  opt_string traceTCPFilename;
  opt_string traceUDPFilename;
  opt_string traceOtherFilename;
  opt_string traceScansFilename;
  opt_string hpFeatureCorrelationFiledir;

  // Cache Simulation Configs:
  std::string POLICY_mode; // config mode covering both replacement and insertion
  std::string POLICY_Replacement;
  std::string POLICY_Insertion;
  bool simMIN, simCache;
  bool POLICY_modeFA;
  bool traceEvictions;
  bool flowRecordTimeseries;

  int simCacheEntries;        // sim.cache.elements
  int simCacheAssociativity;  // sim.cache.associativity

  // wstp link:
  bool enable_wstp;

  // Hashed Perceptron Configs:
  bool ENABLE_Perceptron_Reuse_Prediction;
  bool ENABLE_Perceptron_Bypass_Prediction;
  bool ENABLE_Perceptron_Global_History;
  bool ENABLE_Static_Threshold;
  int Perceptron_DeadBlock_Alpha;
  int Perceptron_Bypass_Alpha;
  double threshold;
  int keep_history;
  int drop_history;
  double samplingRatio;
//  bool ENABLE_History_Training = false;

  // Positive Reinforcement:
//  bool ENABLE_Hit_Training = true;             // (+) on every cache hit
//  bool ENABLE_MRU_Promotion_Training = false;  // (+) when promoted to MRU
//  bool ENABLE_Belady_KeepSet_Training = false; // (+) for Belady's keepSet

  // Negative Reinforcement:
//  bool ENABLE_Eviction_Training = true;  // (-) on eviction without touch
//  bool ENABLE_Belady_EvictSet_Training = true; // (-) for Belady evictSet

  // Corrective Training:
//  bool ENABLE_EoL_Hit_Correction = true;  // (+) on incorrect eol mark

  // Belady's algorithm:
//  bool ENABLE_Belady = true;

  // WSTP Configs:
//  opt_port_ wstp_port;
};


#endif // GLOBAL_CONFIG_H
