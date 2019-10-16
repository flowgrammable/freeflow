//#include "caida-pcap.hpp"

#include "util_caida.hpp"
#include "util_extract.hpp"
#include "util_bitmask.hpp"
#include "util_io.hpp"
#include "sim_min.hpp"
#include "sim_lru.hpp"
#include "cache_sim.hpp"
//#include "util_mem.h"

#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <ctime>
#include <signal.h>
#include <type_traits>
#include <bitset>
#include <map>
#include <tuple>
#include <functional>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <chrono>
#include <memory>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"

// Wolfram Symbolic Transfer Protocol (WSTP):
#include "wstp.hpp"

using namespace std;
namespace po = boost::program_options;

using flow_id_t = u64;

// Global Runtime Timekeeping:
chrono::system_clock::time_point startTime = chrono::system_clock::now();

// Signal Handler:
static bool running = true;
void
sig_handle(int sig) {
  running = false;
}

using opt_string_ = boost::optional<string>;
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
} CONFIG;


string get_time(chrono::system_clock::time_point tp) {
  stringstream ss;
  time_t time_c = chrono::system_clock::to_time_t(tp);
  ss << std::put_time(std::localtime(&time_c),"%F_%T");
  return ss.str();
}

void print_usage(const char* argv0, std::vector<po::options_description> opts) {
  cout << "Usage: " << argv0 << " [options] [input_pcap_files...]\n";
  for (const auto& opt : opts) {
    cout << opt << '\n';
  }
  cout.flush();
}

// Boost commandline and config file parsing.
po::variables_map parse_options(int argc, const char* argv[]) {
  // Get current time (to generate trace filename):
  const std::string startTime_str = get_time(startTime);

  po::options_description explicitOpts("Options");
  explicitOpts.add_options()
    ("help,h", "Print usage (this message)")
    ("config,c",
      po::value<std::vector<string>>(),
      "Input config file")
    ("flow-log,l",
      po::value<opt_string_>(&CONFIG.logFilename)->implicit_value(startTime_str+".log"),
      "Output log file")
    ("decode-log,d",
      po::value<opt_string_>(&CONFIG.decodeLogFilename)->implicit_value(startTime_str+".decode.log"),
      "Packet decode log file")
    ("stats,s",
      po::value<opt_string_>(&CONFIG.statsFilename)->implicit_value(startTime_str+".stats"),
      "Output flows stats")
      // TODO: repurpose flows file...
    ("flows,f",
      po::value<opt_string_>(&CONFIG.flowsFilename)->implicit_value(startTime_str+".flows"),
      "Output observed flows and keys")
    ("trace,t",
      po::value<opt_string_>(&CONFIG.traceFilename)->implicit_value(startTime_str+".trace"),
      "Output timeseries trace (all protocols)")
    ("trace-tcp",
      po::value<opt_string_>(&CONFIG.traceTCPFilename)->implicit_value(startTime_str+".trace.tcp"),
      "Output timeseries trace (TCP only)")
    ("trace-udp",
      po::value<opt_string_>(&CONFIG.traceUDPFilename)->implicit_value(startTime_str+".trace.udp"),
      "Output timeseries trace (UDP only)")
    ("trace-other",
      po::value<opt_string_>(&CONFIG.traceOtherFilename)->implicit_value(startTime_str+".trace.other"),
      "Output timeseries trace (excluding TCP and UDP)")
    ("trace-scans",
      po::value<opt_string_>(&CONFIG.traceScansFilename)->implicit_value(startTime_str+".trace.scans"),
      "Output timeseries trace (assumed TCP Scans)")
    ("timeseries",
      po::value<bool>()->default_value(false)->implicit_value(true),
      "Enable timeseries recording in FlowRecord");

  po::options_description hiddenOpts("Hidden Options");
  hiddenOpts.add_options()
    ("pcaps", po::value<std::vector<string>>(), "PCAP input files")
    ("sim.min", po::value<bool>()->default_value(false), "Enable MIN simulation")
    ("sim.min.entries", po::value<int>()->default_value(1<<10), "Cache elements for MIN simulation")
    ("sim.cache", po::value<bool>()->default_value(false), "Enable cache simulation")
    ("sim.cache.entries", po::value<int>()->default_value(1<<10), "Cache elements for cache simulation")
    ("sim.cache.associativity", po::value<int>()->default_value(8), "Number of ways in cache simulation")
    ("sim.cache.insertion", po::value<int>()->default_value(0), "Insertion postion for cache simulation")
    ("sim.cache.replacement", po::value<int>()->default_value(7), "Replacement position for cache simulation");


  po::positional_options_description positionalOpts;
  positionalOpts.add("pcaps", -1);

  po::options_description allOpts;
  allOpts.add(explicitOpts).add(hiddenOpts);
  po::variables_map vm;
  try {
    /* Stores in 'vm' all options that are defined in 'options'.
     * If 'vm' already has a non-defaulted value of an option, that value is not
     * changed, even if 'options' specify some value.
     */
    // Parse command line first:
    po::store(po::command_line_parser(argc, argv)
              .options(allOpts)
              .positional(positionalOpts)
              .run(), vm);
    // Then parse any config files:
    // - Store's priority apply to config variable as well...
    if (vm.count("config") > 0) {
      auto configFiles = vm["config"].as<vector<string>>();
      for (const string& conf : configFiles) {
        auto tmp = po::parse_config_file<char>(conf.c_str(), allOpts);
        po::store(tmp, vm);
      }
    }
    po::notify(vm);
  }
  catch (po::unknown_option ex) {
    cout << ex.what() << endl;
    print_usage(argv[0], {explicitOpts});
    exit(EXIT_FAILURE);
  }
  if (vm.count("help")) {
    print_usage(argv[0], {explicitOpts});
    exit(EXIT_SUCCESS);
  }
  // Ensure at least one input file is given
  if (!vm.count("pcaps")) {
    cout << "An input file is required!" << endl;
    print_usage(argv[0], {explicitOpts, hiddenOpts});
    exit(EXIT_FAILURE);
  }
  return vm;
}


/// MAIN ///////////////////////////////////////////////////////////////////////
int main(int argc, const char* argv[]) {
  signal(SIGINT, sig_handle);
  signal(SIGKILL, sig_handle);
  signal(SIGHUP, sig_handle);

  po::variables_map config = parse_options(argc, argv);

  // Output Files:
  ofstream NULL_OFSTREAM;
  vector<gz_ostream> gz_handles;

  // gzip file open helper lambda:
  auto open_gzostream = [&](boost::optional<string> opt) -> ostream& {
    if (!opt) {
      return NULL_OFSTREAM;
    }
    string& filename = *opt;
    if (filename.substr(filename.find_last_of('.')) != ".gz") {
      filename.append(".gz");
    }
    gz_handles.emplace_back(filename);
    return gz_handles.back().get_ostream();
  };

  // Configuration:
  constexpr bool ENABLE_simMIN = false;
  constexpr bool ENABLE_simCache = true;
  constexpr bool ENABLE_WSTP = false;
//  constexpr bool ENABLE_FLOW_LOG = false;
  //  const bool enable_simMIN(config["sim.min"].as<bool>());
  //  const bool enable_simCache(config["sim.cache"].as<bool>());


  // Log/Trace Output Files:
  ofstream debugLog(*CONFIG.logFilename); // TODO: Allow no log?
  for (int i = 0; i < argc; i++) {
    debugLog << argv[i] << ' ';
  }
  debugLog << '\n';

  ostream& decodeLog = open_gzostream(CONFIG.decodeLogFilename);
  if (CONFIG.decodeLogFilename) {
    debugLog << "Packet Decode Log File: " << *CONFIG.decodeLogFilename << '\n';
  }

  // Flow Stats Output Files:
  ostream& flowStats = open_gzostream(CONFIG.statsFilename);
  if (CONFIG.statsFilename) {
    debugLog << "Flow Stats File: " << *CONFIG.statsFilename << '\n';
  }
  ostream& flowLog = open_gzostream(CONFIG.flowsFilename);
  if (CONFIG.flowsFilename) {
    debugLog << "Flow Heuristic Log File: " << *CONFIG.flowsFilename << '\n';
  }

  // Trace Output Files:
  ostream& flowTrace = open_gzostream(CONFIG.traceFilename);
  ostream& tcpFlowTrace = open_gzostream(CONFIG.traceTCPFilename);
  ostream& udpFlowTrace = open_gzostream(CONFIG.traceUDPFilename);
  ostream& otherFlowTrace = open_gzostream(CONFIG.traceOtherFilename);
  ostream& scanFlowTrace = open_gzostream(CONFIG.traceScansFilename);
  if (CONFIG.traceFilename) {
    constexpr auto FKT_SIZE = std::tuple_size<FlowKeyTuple>::value;
    debugLog << "Packet Trace File: " << *CONFIG.traceFilename << "\n"
             << "- Flow Key String: " << FKT_SIZE << "B\n";
    debugLog << "TCP Packet Trace File: " << *CONFIG.traceTCPFilename << "\n"
             << "- Flow Key String: " << FKT_SIZE << "B\n"
             << "- Flags: " << sizeof(Flags) << "B\n";
    debugLog << "UDP Packet Trace File: " << *CONFIG.traceUDPFilename << "\n"
             << "- Flow Key String: " << FKT_SIZE << "B\n";
    debugLog << "Untracked Packet Trace File: " << *CONFIG.traceOtherFilename << "\n"
             << "- Flow Key String: " << FKT_SIZE << "B\n";
    debugLog << "Scan Packet Trace File: " << *CONFIG.traceScansFilename << "\n"
             << "- Flow Key String: " << FKT_SIZE << "B\n"
             << "- Flags: " << sizeof(Flags) << "B\n";
  }

  // Open Pcap Input Files:
  caidaHandler caida;
  {
    vector<string> inputs = config["pcaps"].as<vector<string>>();
    for (size_t i = 0; i < inputs.size(); i++) {
      string& filename = inputs[i];
      debugLog << "PCAP stream "<< i << ": " << filename << '\n';
      caida.open(i, filename);
    }
  }
  debugLog.flush();


  //////////////////////////////////////////////////////////////////////////////
  /// Flow State Tracking:
  const FlowRecord::MODE_EN CONFIG_TIMESERIES = config["timeseries"].as<bool>()
      ? FlowRecord::TIMESERIES : FlowRecord::NORMAL;
  flow_id_t nextFlowID = 1; // flowID 0 is reserved for 'flow not found'
  // Maps flows to vector (from most-recent to least-recent) of flowIDs
  absl::flat_hash_map<FlowKeyTuple, absl::InlinedVector<flow_id_t,1>> flowIDs;
  std::map<flow_id_t, FlowRecord> flowRecords;

  // Auxilary Flow State Tracking:
  std::unordered_set<flow_id_t> blacklistFlows;
  std::set<flow_id_t> touchedFlows;

  // Retired Records:
  std::mutex mtx_RetiredFlows;
  std::map<flow_id_t, const FlowRecord> retiredRecords;


  //////////////////////////////////////////////////////////////////////////////
  /// Simulation bookeeping structures:
  SimMIN<flow_id_t> simMIN(config["sim.min.entries"].as<int>());
  // TODO: find way of defining polices...
  CacheSim<flow_id_t> simCache(config["sim.cache.entries"].as<int>(),
                               config["sim.cache.associativity"].as<int>());
//  Policy replacePolicy(Policy::PolicyType::BURST_LRU);
  Policy replacePolicy(Policy::PolicyType::SRRIP);
  simCache.set_replacement_policy(replacePolicy);

//  Policy insertPolicy(Policy::MRU);
//  insertPolicy.offset_ = 2;
//  Policy insertPolicy(Policy::PolicyType::SHIP);
//  simCache.set_insert_policy(insertPolicy);

  std::mutex mtx_misses; // covers both missesMIN and missesCache
  std::map<flow_id_t, std::vector<uint64_t>> missesMIN;
  std::map<flow_id_t, std::vector<uint64_t>> missesCache;

//  using Reservation = typename CacheSim<flow_id_t>::Reservation;
//  using HitStats = typename CacheSim<flow_id_t>::HitStats;
  // TODO, place these cache sim / stats types in a namespace
  using Lifetime = std::tuple<Reservation, HitStats>;
  std::map<flow_id_t, std::vector<Lifetime>> minLifetimes;
  std::map<flow_id_t, std::vector<Lifetime>> cacheLifetimes;


  // Lamba function interface for reporting misses:
  auto f_sort_by_misses = [&mtx_RetiredFlows, &retiredRecords]
      (const std::map<flow_id_t, std::vector<uint64_t>>& misses) {
    using pq_pair_t = std::pair<size_t, flow_id_t>;
    using pq_container_t = std::vector<pq_pair_t>;
    auto pq_compare_f = [](const pq_pair_t& a, const pq_pair_t& b) {
      // vector is popped from end, so most important items should be at end..
      if (a.first == b.first)
        return a.second > b.second; // oldest flow (smalled flow_id at end)
      return a.first < b.first;     // largest misses at end
    };

    pq_container_t queue;
    { lock_guard lock(mtx_RetiredFlows);
      for (const auto& [fid, flowR] : retiredRecords) {
        auto it = misses.find(fid);
        if (it != misses.end()) {
          const auto& fmv = it->second;
          queue.push_back(std::make_pair(fmv.size(), fid));
        }
        else {
          queue.push_back(std::make_pair(0, fid));
        }
      }
    }
//    { lock_guard lck(mtx_Misses);
//      for (const auto& [fid, fmv] : misses) {
//        if (retiredRecords.count(fid) > 0) {
//          queue.push_back(std::make_pair(fmv.size(), fid));
//        }
//      }
//    }
    std::stable_sort(queue.begin(), queue.end(), pq_compare_f);
    return queue;
  };


//  std::vector<int64_t> histBurst;
//  auto burstHistogram = [&histBurst](const auto& ts) {
//    for (auto burst : ts) {
//      if (burst >= histBurst.size()) {
//        histBurst.resize(burst*2);
//      }
//      histBurst[burst]++;
//    }
//  };

  /// Helper functions to interact with cache simulations: /////////////////////
  // - Insert called once (first packet on flow startup)
  std::function<void(flow_id_t, const std::pair<uint64_t,timespec>&&)> f_sim_insert =
      [&](flow_id_t flowID, const std::pair<uint64_t,timespec>&& pktTime) {
    auto [ns, ts] = pktTime;
    if (ENABLE_simMIN) {
      simMIN.insert(flowID, ts);
    }

    if (ENABLE_simCache) {
      auto victim = simCache.insert(flowID, ts);
      if (ENABLE_WSTP && victim) {
        // If victim exists (something was replaced).
        // victim: std::tuple<Key, Reservation, HitStats>
        auto& [id, res, hits] = *victim;
        cacheLifetimes[id].emplace_back(res, hits);
      }
    }
  };

  // - Update called on each subsequent packet
  std::function<void(flow_id_t, const std::pair<uint64_t,timespec>&&)> f_sim_update =
      [&](flow_id_t flowID, const std::pair<uint64_t,timespec>&& pktTime) {
    auto [ns, ts] = pktTime;
    if (ENABLE_simMIN) {
//      auto [hit, evictSet] = simMIN.update(flowID, ts);
      bool hit = simMIN.update(flowID, ts);
      auto evictSet = simMIN.evictions();
      if (ENABLE_WSTP && !hit) {
        std::lock_guard lock(mtx_misses);
        missesMIN[flowID].emplace_back(ns);
      }
    }

    if (ENABLE_simCache) {
      auto [hit, victim] = simCache.update(flowID, ts);
      if (ENABLE_WSTP && !hit) {
      // TODO: take note of miss statistics...
        std::lock_guard lock(mtx_misses);
        missesCache[flowID].emplace_back(ns);
        if (victim) {
          // If victim exists (something was replaced).
          // victim: std::tuple<Key, Reservation, HitStats>
          auto& [id, res, hits] = *victim;
          cacheLifetimes[id].emplace_back(res, hits);
        }
      }
    }
  };


  // Generic Packet Stats:
  u16 maxPacketSize = std::numeric_limits<u16>::lowest();
  u16 minPacketSize = std::numeric_limits<u16>::max();
  u64 totalPackets = 0, totalBytes = 0, \
      blacklistPackets = 0, malformedPackets = 0, timeoutPackets = 0, \
      flowPortReuse = 0;

  /// Filtering function to find 'interesting' flows once they retire: /////////
  std::function<void(FlowRecord&&)> f_sample = [&](FlowRecord&& r) {
    // Only consider semi-large flows:
    if (ENABLE_WSTP) {
      const auto duration = r.last();
      if (duration.second.tv_sec > 1 && r.packets() >= 32) {
  //    if (r.packets() > 512) {
        std::unique_lock missesLock(mtx_misses);
        auto cache_it = missesCache.find(r.getFlowID());
        auto min_it = missesMIN.find(r.getFlowID());
        missesLock.unlock();
        const auto& cache_ts = cache_it->second;
        const auto& min_ts = min_it->second;

        // Only record multiple misses within MIN:
        if ( min_it != missesMIN.end() && min_ts.size() >= 8 ) {
          // Flow looks interesting, add to retiredRecords:
  //        std::cout << "+ FlowID: " << r.getFlowID()
  //                  << ", Packets: " << r.packets()
  //                  << ", Misses: " << min_ts.size()
  //                  << "; " << print_flow_key_string( r.getFlowTuple() )
  //                  << std::endl;
          { std::lock_guard lock(mtx_RetiredFlows);
            retiredRecords.emplace(r.getFlowID(), r);
            // keep flow in missesCache, missesMIN, and cacheLifetimes
          }
          return;
        }
      }
      // Flow behaved unremarkibly, delete metadata:
      {
        std::lock_guard lock(mtx_misses);
        missesCache.erase(r.getFlowID());
        missesMIN.erase(r.getFlowID());
        cacheLifetimes.erase(r.getFlowID());
      }
    }
    // FlowRecord: r is implicitly destroyed (r-value reference)
  };


  /// WSTP interface functions used for exploration ////////////////////////////
  wstp_link::fn_t f_get_misses_MIN = [&](wstp_link::arg_t) {
    static auto sorted_queue = f_sort_by_misses(missesMIN);
    static size_t update_triger = sorted_queue.size();
    if (update_triger != retiredRecords.size()) {
      sorted_queue = f_sort_by_misses(missesMIN);
      update_triger = sorted_queue.size();
    }
    if (update_triger == 0) {
      std::cerr << "No flows in retiredRecrods." << std::endl;
      return wstp_link::arg_t();
    }

    // Lock and pop a sample:
    auto [m_count, m_fid] = sorted_queue.back(); sorted_queue.pop_back();
    std::cout << "Misses: " << m_count << "; FID: " << m_fid << std::endl;
    std::unique_lock lck(mtx_RetiredFlows);
    auto record = retiredRecords.extract(m_fid);
    lck.unlock();
    if (record) {
      std::cerr << "Missing FID (" << m_fid << ") in retiredRecrods." << std::endl;
      return wstp_link::arg_t();
    }
    update_triger--;

    // Calculate event deltas deltas:
    const FlowRecord& flowR = record.mapped();
    auto& ts = flowR.getArrivalV();
    wstp_link::ts_t deltas(ts.size());
    std::adjacent_difference(ts.begin(), ts.end(), deltas.begin());

    // Lock and search for miss vector:
    flow_id_t fid = flowR.getFlowID();
    std::unique_lock lck2(mtx_misses);
    auto miss_ts = missesMIN.extract(fid);
    lck2.unlock();
    if (miss_ts.empty()) {
      return wstp_link::arg_t(deltas);
    }

    // Mark miss by returning netagive distances:
    for (auto miss : miss_ts.mapped()) {
      auto x = std::find(ts.begin(), ts.end(), miss);
      deltas[std::distance(ts.begin(), x)] *= -1;
    }
    return wstp_link::arg_t(deltas);
  };

  // Sampling of retired flows:
  wstp_link::fn_t f_get_arrival = [&](wstp_link::arg_t) {
    if (retiredRecords.size() == 0) {
      return wstp_link::ts_t();
    }
    // Lock and pop:
    std::unique_lock lck(mtx_RetiredFlows);
    auto record = retiredRecords.extract(retiredRecords.begin());
    lck.unlock();
    // Calculate deltas and return:
    auto& ts = record.mapped().getArrivalV();
    wstp_link::ts_t deltas(ts.size());
    std::adjacent_difference(ts.begin(), ts.end(), deltas.begin());
    return deltas;
  };

  wstp_link::fn_t f_num_flows = [&](wstp_link::arg_t) {
    return retiredRecords.size();
  };

  wstp_link::fn_t f_get_ids = [&](wstp_link::arg_t) {
    wstp_link::ts_t ids;  // reusing timeseries type for vector of 64-bit ints
    for (const auto& [key, value] : retiredRecords) {
      ids.push_back(key);
    }
    // BUG: confirm why 1 record is missing on send...
    return ids;
  };

  wstp_link::fn_t f_get_ts_deltas = [&](wstp_link::arg_t v) {
    auto fid = std::get<int64_t>(v);
    // Calculate event deltas deltas:
    const FlowRecord& flowR = retiredRecords.at(fid);
    const auto& ts = flowR.getArrivalV();
    wstp_link::ts_t deltas(ts.size());
    std::adjacent_difference(ts.begin(), ts.end(), deltas.begin());

    // Search for miss vector:
    std::unique_lock lck2(mtx_misses);
    auto miss_ts = missesMIN.at(fid);
    lck2.unlock();
    if (miss_ts.empty()) {
      return wstp_link::arg_t(deltas);
    }

    // Mark miss by returning netagive distances:
    for (auto miss : miss_ts) {
      auto x = std::find(ts.begin(), ts.end(), miss);
      deltas[std::distance(ts.begin(), x)] *= -1;
    }
    return wstp_link::arg_t(deltas);
  };

  wstp_link::fn_t f_get_ts = [&](wstp_link::arg_t v) {
    auto fid = std::get<int64_t>(v);
    const FlowRecord& flowR = retiredRecords.at(fid);
    const auto& ts = flowR.getArrivalV();
    wstp_link::ts_t ret(ts.begin(), ts.end());
    return ret;
  };

  wstp_link::fn_t f_get_ts_miss_MIN = [&](wstp_link::arg_t v) {
    auto fid = std::get<int64_t>(v);
    wstp_link::ts_t ts;
    try {
      std::lock_guard lock(mtx_misses);
      auto miss_ts = missesMIN.at(fid);
      ts = wstp_link::ts_t(miss_ts.begin(), miss_ts.end());
    }
    catch (std::out_of_range) {}
    return ts;
  };

  wstp_link::fn_t f_get_ts_miss_SIM = [&](wstp_link::arg_t v) {
    auto fid = std::get<int64_t>(v);
    wstp_link::ts_t ts;
    try {
      std::lock_guard lock(mtx_misses);
      auto miss_ts = missesCache.at(fid);
      ts = wstp_link::ts_t(miss_ts.begin(), miss_ts.end());
    }
    catch (std::out_of_range) {}
    return ts;
  };

  wstp_link::fn_t f_get_samples = [&](wstp_link::arg_t v) {
    auto n = std::get<int64_t>(v);
    wstp_link::data_t samples;
    for (const auto& [key, value] : retiredRecords) {
      if (n-- == 0) { break; }
      wstp_link::ts_t s = std::get<wstp_link::ts_t>(f_get_ts_deltas(key));
      samples.push_back(s);
    }
    return samples;
  };

  // Instantiate Mathematica Link:
  wstp_singleton& wstp = wstp_singleton::getInstance();
  if (ENABLE_WSTP) {
    using def_t = wstp_link::def_t;
    using fn_t = wstp_link::fn_t;
    using sig_t = wstp_link::sig_t;
    wstp_link::register_fn( def_t(sig_t("FFNumFlows[]", ""), fn_t(f_num_flows)) );
    wstp_link::register_fn( def_t(sig_t("FFSampleFlow[]", ""), fn_t(f_get_arrival)) );
    wstp_link::register_fn( def_t(sig_t("FFGetMisses[]", ""), fn_t(f_get_misses_MIN)) );
    wstp_link::register_fn( def_t(sig_t("FFGetFlowIDs[]", ""), fn_t(f_get_ids)) );
    wstp_link::register_fn( def_t(sig_t("FFGetFlowTS[x_Integer]", "x"), fn_t(f_get_ts)) );
    wstp_link::register_fn( def_t(sig_t("FFGetFlowDelta[x_Integer]", "x"), fn_t(f_get_ts_deltas)) );
    wstp_link::register_fn( def_t(sig_t("FFGetFlowMissMIN[x_Integer]", "x"), fn_t(f_get_ts_miss_MIN)) );
    wstp_link::register_fn( def_t(sig_t("FFGetFlowMissSIM[x_Integer]", "x"), fn_t(f_get_ts_miss_SIM)) );
    wstp_link::register_fn( def_t(sig_t("FFGetSamples[x_Integer]", "x"), fn_t(f_get_samples)) );
    string interface = wstp.listen();
    debugLog << "WSTP Server listening on " << interface << endl;
  }


  /// PCAP file read loop: /////////////////////////////////////////////////////
  s64 count = 0, last_count = 0;
  fp::Context cxt = caida.recv();
  timespec last_epoc = cxt.packet().timestamp();
  while (running && cxt.packet_ != nullptr) {
    flow_id_t flowID;
    const timespec& pktTime = cxt.packet().timestamp();

    { // per-packet processing
      const fp::Packet& p = cxt.packet();
      EvalContext evalCxt(p);

      // Attempt to parse packet headers:
      count++;
      try {
        int status = extract(evalCxt, IP);
      }
      catch (std::exception& e) {
        stringstream ss;
        ss << count << " (" << p.size() << "B): ";
        ss << evalCxt.v.absoluteBytes<int>() << ", "
           << evalCxt.v.committedBytes<int>() << ", "
           << evalCxt.v.pendingBytes<int>() << '\n';
        ss << dump_packet(p).str() << '\n';
        print_eval(ss, evalCxt);
        ss << "> Exception occured during extract: " << e.what();
        decodeLog << ss.str() << endl;
        malformedPackets++;
      }

      // Associate packet with flow:
      const Fields& k = evalCxt.fields;
      const FlowKeyTuple fkt = make_flow_key_tuple(k);

      // Update global stats:
      u16 wireBytes = evalCxt.origBytes;
      maxPacketSize = std::max(maxPacketSize, wireBytes);
      minPacketSize = std::min(minPacketSize, wireBytes);
      totalBytes += wireBytes;
      totalPackets++;


      // Perform initial flowID lookup:
      auto flowID_it = flowIDs.find(fkt);
      flowID = (flowID_it != flowIDs.end()) ? flowID_it->second.front() : 0;

      ////////////////////////////////////
      // Trace output generation lambda:
      auto tracePoint = [&]() {
        const auto& fields = evalCxt.fields;
        const auto& proto = fields.fProto;

        // if flow is established?...
        // TODO: how to punt suspected scans to seperate trace file?

        if (proto & ProtoFlags::isTCP) {
          serialize(tcpFlowTrace, fkt);
          serialize(tcpFlowTrace, make_flags_bitset(k));
        }
        else if (proto & ProtoFlags::isUDP) {
          serialize(udpFlowTrace, fkt);
        }
        else {
          serialize(otherFlowTrace, fkt);
        }
        serialize(flowTrace, fkt);
      };
      ////////////////////////////////////
      tracePoint();


      // If flow exists:
      if (flowID != 0) {
        // Known Flow:
        auto flowRecord_it = flowRecords.find(flowID); // iterator to {key, value}
        if (flowRecord_it != flowRecords.end()) {
          std::reference_wrapper<FlowRecord> flowR = flowRecord_it->second;

          // TODO: ignore syn retransmits...?
          // Ensure packet doesn't correspond to a new flow (port-reuse):
          if ( evalCxt.fields.fProto & ProtoFlags::isTCP &&
               (evalCxt.fields.fTCP & TCPFlags::SYN) == TCPFlags::SYN &&
                evalCxt.fields.tcpSeqNum != flowR.get().lastSeq() ) {
            // Packet indicates new flow:
            timespec sinceLast = pktTime - flowR.get().last().second;
            flowLog << (flowR.get().isTCP()?"TCP":"???") << " flow " << flowID
                    << " considered terminated after new SYN, "
                    << flowR.get().packets()
                    << " packets and " << sinceLast.tv_sec
                    << " seconds before port reuse; "
                    << print_flow_key_string(k) << endl;

            // Retire old record:
            flowStats << print_flow(flowR).str();
            flowRecords.erase(flowRecord_it);

            // Create new record for new flow:
            flowID = nextFlowID++;
            auto& flowID_vector = flowID_it->second;
            flowID_vector.insert(flowID_vector.begin(), flowID);

            auto newFlowRecord = flowRecords.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(flowID),
                                  std::forward_as_tuple(flowID, fkt, pktTime, CONFIG_TIMESERIES));
            assert(newFlowRecord.second); // sanity check
            flowR = newFlowRecord.first->second;
            flowPortReuse++;
            flowR.get().update(evalCxt);
            touchedFlows.insert(flowID);
            f_sim_insert(flowID, flowR.get().last());
          }
          else {
            flowR.get().update(evalCxt);
            touchedFlows.insert(flowID);
            f_sim_update(flowID, flowR.get().last());
          }
        }
        else {
          // Flow was seen before but no longer tracked:
          if (blacklistFlows.find(flowID) != blacklistFlows.end()) {
            serialize(scanFlowTrace, fkt);
            serialize(scanFlowTrace, make_flags_bitset(k));
            blacklistPackets++;
          }
          else {
            // FIXME: properly seperate new flow record creation!
            // Ensure packet doesn't correspond to a new flow (port-reuse):
            if ( evalCxt.fields.fProto & ProtoFlags::isTCP &&
                 (evalCxt.fields.fTCP & TCPFlags::SYN) == TCPFlags::SYN ) {
              // Packet indicates new flow:
//              timespec sinceLast = pktTime - flowR.get().last();
//              debugLog << (flowR.get().isTCP()?"TCP":"???") << " flow " << flowID
//                       << " considered terminated after new SYN, "
//                       << flowR.get().packets()
//                       << " packets and " << sinceLast.tv_sec
//                       << " seconds before port reuse; "
//                       << print_flow_key_string(k) << endl;

              // Create new record for new flow:
              flowID = nextFlowID++;
              auto& flowID_vector = flowID_it->second;
              flowID_vector.insert(flowID_vector.begin(), flowID);

              auto newFlowRecord = flowRecords.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(flowID),
                                    std::forward_as_tuple(flowID, fkt, pktTime, CONFIG_TIMESERIES));
              assert(newFlowRecord.second); // sanity check
              std::reference_wrapper<FlowRecord> flowR = newFlowRecord.first->second;
              flowPortReuse++;

              flowR.get().update(evalCxt);
              touchedFlows.insert(flowID);
              f_sim_insert(flowID, flowR.get().last());
            }
            else {
              flowLog << "FlowID " << flowID << " is no longer being tracked; "
                      << print_flow_key_string(k) << endl;
              timeoutPackets++;
              f_sim_update(flowID, std::make_pair(0, pktTime)); // FlowRecord was reclaimed!...
            }
          }
        }
      }
      else {
        // Unkown Flow:
        auto newFlowID = flowIDs.emplace(std::piecewise_construct,
            std::forward_as_tuple(fkt),
            std::forward_as_tuple(std::initializer_list<flow_id_t>({nextFlowID})) );
        assert(newFlowID.second); // sanity check
        flowID = nextFlowID++;

        // Filter out TCP scans (first packet observed has RST):
        if (evalCxt.fields.fProto & ProtoFlags::isTCP &&
            (evalCxt.fields.fTCP & TCPFlags::RST ||
             evalCxt.fields.fTCP & TCPFlags::FIN) ) {
          serialize(scanFlowTrace, fkt);
          serialize(scanFlowTrace, make_flags_bitset(k));
          blacklistFlows.insert(flowID);
  //        debugLog << "Blacklisting TCP flow " << flowID
  //                 << "; first packet observed was "
  //                 << ((evalCxt.fields.fTCP & TCPFlags::FIN)?"FIN":"RST")
  //                 << endl;
        }
        else {
          // Follow new flow:
          auto newFlowRecord = flowRecords.emplace(std::piecewise_construct,
                                std::forward_as_tuple(flowID),
                                std::forward_as_tuple(flowID, fkt, pktTime, CONFIG_TIMESERIES));
          assert(newFlowRecord.second); // sanity check
          auto& FlowR = newFlowRecord.first->second;
          FlowR.update(evalCxt);
          f_sim_insert(flowID, FlowR.last());
        }

        ///////////////////////////////////////////////////
        // Clean up dormant flows every new 128k new flows:
        // TODO: make trace not thotime based?
        if (flowID % (128*1024) == 0) {
          timespec diff = pktTime - last_epoc;
          last_epoc = pktTime;
          auto diff_pkts = count - last_count;
          last_count = count;
          cout << touchedFlows.size() << "/" << flowRecords.size()
               << " flows touched (" << diff_pkts
               << " packets) since last epoch spanning "
               << diff.tv_sec + double(diff.tv_nsec)/double(1000000000.0)
               << " seconds."
               << endl;

          // Add flows not observed to dormant list:
          std::set<flow_id_t> dormantFlows;
          std::set_difference(
                KeyIterator(flowRecords.begin()), KeyIterator(flowRecords.end()),
                touchedFlows.begin(), touchedFlows.end(),
                std::inserter(dormantFlows, dormantFlows.end()) );

          touchedFlows.clear();

          // Scan flows which have signaled FIN/RST:
          for (auto i : dormantFlows) {
            // Timeout units in seconds.
            constexpr int64_t RST_TIMEOUT = 10;
            constexpr int64_t FIN_TIMEOUT = 60;
            constexpr int64_t TCP_TIMEOUT = 600;
            constexpr int64_t LONG_TIMEOUT = 120;

            /////////////////////
            // Delete Flow lambda
            // - Removes flow from tracked flowRecords, samples if noteworthy.
            auto delete_flow = [&flowRecords, &f_sample](decltype(flowRecords)::iterator flowR_i) {
              f_sample( std::move(flowRecords.extract(flowR_i).mapped()) );
            };

            auto flowR_i = flowRecords.find(i);
            const FlowRecord& flowR = flowR_i->second;

            timespec sinceLast = pktTime - flowR.last().second;
            if (flowR.sawRST() && sinceLast.tv_sec >= RST_TIMEOUT) {
              flowLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                      << " considered terminated after RST, "
    //                    << (flowR.sawFIN()?"FIN, ":"RST, ")
                      << flowR.packets()
                      << " packets and " << sinceLast.tv_sec
                      << " seconds of inactivity; "
                      << print_flow_key_string( flowR.getFlowTuple() ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
            else if (flowR.sawFIN() && sinceLast.tv_sec >= FIN_TIMEOUT) {
              flowLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                      << " considered terminated after FIN, "
    //                    << (flowR.sawFIN()?"FIN, ":"RST, ")
                      << flowR.packets()
                      << " packets and " << sinceLast.tv_sec
                      << " seconds of inactivity; "
                      << print_flow_key_string( flowR.getFlowTuple() ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
            else if (flowR.isTCP() && sinceLast.tv_sec >= TCP_TIMEOUT) {
              flowLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                      << " considered dormant after " << flowR.packets()
                      << " packets and " << sinceLast.tv_sec
                      << " seconds of inactivity; "
                      << print_flow_key_string( flowR.getFlowTuple() ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
            else if (sinceLast.tv_sec >= LONG_TIMEOUT) {
              flowLog << (flowR.isUDP()?"UDP":"???") << " flow " << i
                      << " considered dormant afer " << flowR.packets()
                      << " packets and " << sinceLast.tv_sec
                      << " seconds of inactivity; "
                      << print_flow_key_string( flowR.getFlowTuple() ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
          }
        } // end flow cleanup
      }
    } // end per-packet processing

    // Release resources and retrieve another packet:
    caida.rebound(&cxt);
    cxt = caida.recv();
  }


  // Print flow tuples:
  for (const auto& flow : flowRecords) {
    const auto& key = flow.first;
    const FlowRecord& record = flow.second;

    flowStats << print_flow(record).str();
//    serialize(flowStats, key);
//    serialize(flowStats, record.getFlowID());
//    serialize(flowStats, record.packets());
    // output other flow statistics?
  }

  {
    auto it = flowRecords.cbegin();
    while ( it != flowRecords.cend() ) {
      auto flowR = flowRecords.extract(it);
      f_sample( std::move(flowR.mapped()) );
      it = flowRecords.cbegin();
    }
  }

  // Print global stats:
  debugLog << "Max packet size: " << maxPacketSize << '\n';
  debugLog << "Min packet size: " << minPacketSize << '\n';
  debugLog << "Total bytes: " << totalBytes << '\n';
  debugLog << "Total packets: " << totalPackets << '\n';
  debugLog << "Total flows: " << flowIDs.size() << '\n';
  debugLog << "Blacklisted flows: " << blacklistFlows.size() << '\n';
  debugLog << "Blacklisted packets: " << blacklistPackets << '\n';
  debugLog << "Timeout packets: " << timeoutPackets << '\n';
  debugLog << "Malformed packets: " << malformedPackets << '\n';
  debugLog << "Flow port reuse: " << flowPortReuse << '\n';

  // Print Cache Simulation Stats:
  // TODO: move print into class function...
  if (ENABLE_simMIN) {
    debugLog << simMIN.print_stats() << '\n';
  }
  if (ENABLE_simCache) {
    debugLog << simCache.print_stats() << '\n';
  }

//  debugLog << "Burst Histogram:\n";
//  for (auto it = histBurst.begin(); it != histBurst.end(); it++) {
//    if (*it != 0)
//      debugLog << std::distance(histBurst.begin(), it) << ": " << *it << '\n';
//  }

  chrono::system_clock::time_point end_time = chrono::system_clock::now();
  auto msec = chrono::duration_cast<chrono::milliseconds>(end_time - startTime).count();
  auto sec  = msec/1000;
  auto min  = sec/60;
  auto hrs  = min/60;
  debugLog << "Run took "
           << hrs << "h:" << min%60 << "m:"
           << sec%60 << "s:" << msec%1000 << "ms" << endl;
//  debugLog << "Capture Start: " << packet_time << '\n'
//           << "Capture End: " << packet_time2 << '\n'
//           << "Capture Duration: " << packet_time2-packet_time << endl;


  for (auto& h : gz_handles) {
    h.flush();
  }

  cout << "Run took "
       << hrs << "h:" << min%60 << "m:"
       << sec%60 << "s:" << msec%1000 << "ms\n";
  cout << "Processed " << totalPackets << " packets ("
       << flowIDs.size() << " flows)." << endl;

  // Simulation finished, wait for uninstall from WSTP:
  wstp.wait_for_unlink();
  cerr << "Main thread terminating." << endl;
  return EXIT_SUCCESS;
}
