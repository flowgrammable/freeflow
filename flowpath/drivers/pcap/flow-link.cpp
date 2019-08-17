//#include "caida-pcap.hpp"

#include "util_caida.hpp"
#include "util_extract.hpp"
#include "util_bitmask.hpp"
#include "util_io.hpp"
#include "sim_min.hpp"
#include "sim_lru.hpp"
#include "cache_sim.hpp"

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
using opt_port_ = boost::optional<uint16_t>;
struct global_config {
  // CAIDA Trace Generation:
  opt_string_ logFilename;
  opt_string_ flowsFilename;
  opt_string_ statsFilename;
  opt_string_ traceFilename;
  opt_string_ traceTCPFilename;
  opt_string_ traceUDPFilename;
  opt_string_ traceOtherFilename;
  opt_string_ traceScansFilename;

  // Cache Simulation Configs:

  // WSTP Configs:
  opt_port_ wstp_port;
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
      po::value<opt_string_>(&CONFIG.logFilename)->default_value(startTime_str+".log"),
      "Output log file")
    ("decode-log,d",
      po::value<opt_string_>(&CONFIG.logFilename)->default_value(startTime_str+".decode.log"),
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
      "Output timeseries trace (assumed TCP Scans)");

  po::options_description hiddenOpts("Hidden Options");
  hiddenOpts.add_options()
    ("pcaps", po::value<vector<string>>(), "PCAP input files");

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
  // - unique_ptr to prevent resizing vector moving gz_ostream.
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
  constexpr bool enable_wstp = true;
//  constexpr bool enable_sim = true;

  // Log/Trace Output Files:
  ofstream debugLog(*CONFIG.logFilename);

  // Flow Stats Output Files:
  ostream& flowStats = open_gzostream(CONFIG.statsFilename);
  if (CONFIG.statsFilename) {
    debugLog << "Flow Stats File: " << *CONFIG.statsFilename << "\n";
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

  // Flow State Tracking:
  flow_id_t nextFlowID = 1; // flowID 0 is reserved for 'flow not found'
  // maps flows to vector (from most-recent to least-recent) of flowIDs
  absl::flat_hash_map<FlowKeyTuple, absl::InlinedVector<flow_id_t,1>> flowIDs;
  std::map<flow_id_t, FlowRecord> flowRecords;
  std::unordered_set<flow_id_t> blacklistFlows;
  std::set<flow_id_t> touchedFlows;

  // Retired Records:
  std::map<flow_id_t, const FlowRecord> retiredRecords;
  std::mutex mtx_RetiredFlows;

  // Simulation bookeeping structures:
#define ENABLE_SIM
#ifdef ENABLE_SIM
  SimMIN<flow_id_t> simMIN(1<<10);  // MIN Algorithm
//  SimLRU<flow_id_t> simLRU(1<<16);  // Fully Associative LRU Algorithm
//  CacheSim<flow_id_t> simLRU_2(1<<16); // REMOVE ME: sanity check
  CacheSim<flow_id_t> cacheSimLRU(1<<10, 8); // 8-way set-associative LRU
  std::mutex mtx_Misses; // covers both missesMIN and missesLRU
  std::map<flow_id_t, std::vector<uint64_t>> missesMIN;
  std::map<flow_id_t, std::vector<uint64_t>> missesLRU;
#endif

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

  /// Helper functions to interact with cache simulations: /////////////////////
  // - Insert called once (first packet on flow startup)
  std::function<void(flow_id_t, const std::pair<uint64_t,timespec>&&)> f_sim_insert =
      [&](flow_id_t flowID, const std::pair<uint64_t,timespec>&& pktTime) {
#ifdef ENABLE_SIM
    auto [ns, ts] = pktTime;
    simMIN.insert(flowID, ts);
//    simLRU.insert(flowID, ts);
//    simLRU_2.insert(flowID, ts);
    auto victim = cacheSimLRU.insert(flowID, ts);
    if (victim) {
      // If victim exists (something was replaced).
      // victim: std::pair<Key, Reservation>
      flow_id_t id = victim->first;
      std::pair<size_t,size_t>& reservation = victim->second;
    }
    { // TODO: revisit clearning misses on new flow insert...
      std::lock_guard lock(mtx_Misses);
      missesMIN.erase(flowID);
      missesLRU.erase(flowID);
    }
#endif
  };
  // - Update called on each subsequent packet
  std::function<void(flow_id_t, const std::pair<uint64_t,timespec>&&)> f_sim_update =
      [&](flow_id_t flowID, const std::pair<uint64_t,timespec>&& pktTime) {
#ifdef ENABLE_SIM
    auto [ns, ts] = pktTime;
    if (!simMIN.update(flowID, ts)) {
      std::lock_guard lock(mtx_Misses);
      missesMIN[flowID].emplace_back(ns);
    }
//    bool test;
//    if (!(test = simLRU.update(flowID, ts))) {
//      std::lock_guard lock(mtx_Misses);
//      missesLRU[flowID].emplace_back(ns);
//    }
//    auto test2 = simLRU_2.update(flowID, ts);
//    assert(test2.first == test);
    auto [hit, victim] = cacheSimLRU.update(flowID, ts);
    if (!hit) {
      // TODO: take note of miss statistics...
      missesLRU[flowID].emplace_back(ns);
      if (victim) {
        // If victim exists (something was replaced).
        // victim: std::pair<Key, Reservation>
        flow_id_t id = victim->first;
        std::pair<size_t,size_t>& reservation = victim->second;
      }
    }
#endif
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
    if (r.packets() > 512) {
      std::unique_lock missesLock(mtx_Misses);
//      auto lru_it = missesLRU.find(r.getFlowID());
      auto min_it = missesMIN.find(r.getFlowID());
      const auto& min_ts = min_it->second;
      // Only record multiple misses within MIN:
      if ( min_it != missesMIN.end() && min_ts.size() >= 8 ) {
        missesLock.unlock();
        // Flow looks interesting, add to retiredRecords:
        std::cout << "+ FlowID: " << r.getFlowID()
                  << ", Packets: " << r.packets()
                  << ", Misses: " << min_ts.size() << std::endl;
        { std::lock_guard lock(mtx_RetiredFlows);
          retiredRecords.emplace(std::make_pair(r.getFlowID(), r));
        }
      }
      else {
        // Flow behaved unremarkibly, delete metadata:
        missesLRU.erase(r.getFlowID());
        missesMIN.erase(r.getFlowID());
        missesLock.unlock();
      }
    }
  };


  /// WSTP interface functions used for exploration ////////////////////////////
  wstp_link::fn_t f_get_misses_MIN = [&](flow_id_t) {
    static auto sorted_queue = f_sort_by_misses(missesMIN);
    static size_t update_triger = sorted_queue.size();
    if (update_triger != retiredRecords.size()) {
      sorted_queue = f_sort_by_misses(missesMIN);
      update_triger = sorted_queue.size();
    }
    if (update_triger == 0) {
      std::cerr << "No flows in retiredRecrods." << std::endl;
      return wstp_link::return_t();
    }

    // Lock and pop a sample:
    auto [m_count, m_fid] = sorted_queue.back(); sorted_queue.pop_back();
    std::cout << "Misses: " << m_count << "; FID: " << m_fid << std::endl;
    std::unique_lock lck(mtx_RetiredFlows);
    auto record = retiredRecords.extract(m_fid);
    lck.unlock();
    if (record) {
      std::cerr << "Missing FID (" << m_fid << ") in retiredRecrods." << std::endl;
      return wstp_link::return_t();
    }
    update_triger--;

    // Calculate event deltas deltas:
    const FlowRecord& flowR = record.mapped();
    auto& ts = flowR.getArrivalV();
    wstp_link::ts_t deltas(ts.size());
    std::adjacent_difference(ts.begin(), ts.end(), deltas.begin());

    // Lock and search for miss vector:
    flow_id_t fid = flowR.getFlowID();
    std::unique_lock lck2(mtx_Misses);
    auto miss_ts = missesMIN.extract(fid);
    lck2.unlock();
    if (miss_ts.empty()) {
      return wstp_link::return_t(deltas);
    }

    // Mark miss by returning netagive distances:
    for (auto miss : miss_ts.mapped()) {
      auto x = std::find(ts.begin(), ts.end(), miss);
      deltas[std::distance(ts.begin(), x)] *= -1;
    }
    return wstp_link::return_t(deltas);
  };

  // Sampling of retired flows:
  wstp_link::fn_t f_get_arrival = [&](uint64_t) {
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

  wstp_link::fn_t f_num_flows = [&](uint64_t) {
    return retiredRecords.size();
  };

  wstp_link::fn_t f_get_ids = [&](uint64_t) {
    wstp_link::ts_t ids;  // reusing timeseries type for vector of 64-bit ints
    for (const auto& [key, value] : retiredRecords) {
      ids.push_back(key);
    }
    return ids;
  };


  // Instantiate Mathematica Link:
  wstp_singleton& wstp = wstp_singleton::getInstance();
  if (enable_wstp) {
    using def_t = wstp_link::def_t;
    using fn_t = wstp_link::fn_t;
    using sig_t = wstp_link::sig_t;
    wstp_link::register_fn( def_t(sig_t("FFNumFlows[]", ""), fn_t(f_num_flows)) );
    wstp_link::register_fn( def_t(sig_t("FFSampleFlow[]", ""), fn_t(f_get_arrival)) );
    wstp_link::register_fn( def_t(sig_t("FFGetMisses[]", ""), fn_t(f_get_misses_MIN)) );
    wstp_link::register_fn( def_t(sig_t("FFGetFlowIDs[]", ""), fn_t(f_get_ids)) );
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
        cerr << ss.str() << endl;
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
      auto flowID_status = flowIDs.find(fkt);
      flowID = (flowID_status != flowIDs.end()) ? flowID_status->second.front() : 0;

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
        auto flowRecord_status = flowRecords.find(flowID); // iterator to {key, value}
        if (flowRecord_status != flowRecords.end()) {
          std::reference_wrapper<FlowRecord> flowR = flowRecord_status->second;

          // TODO: ignore syn retransmits...?
          // Ensure packet doesn't correspond to a new flow (port-reuse):
          if ( evalCxt.fields.fProto & ProtoFlags::isTCP &&
               (evalCxt.fields.fTCP & TCPFlags::SYN) == TCPFlags::SYN &&
               evalCxt.fields.tcpSeqNum != flowR.get().lastSeq() ) {
            // Packet indicates new flow:
            timespec sinceLast = pktTime - flowR.get().last().second;
            debugLog << (flowR.get().isTCP()?"TCP":"???") << " flow " << flowID
                     << " considered terminated after new SYN, "
                     << flowR.get().packets()
                     << " packets and " << sinceLast.tv_sec
                     << " seconds before port reuse; "
                     << print_flow_key_string(k) << endl;

            // Retire old record:
            flowStats << print_flow(flowR).str();
            flowRecords.erase(flowRecord_status);

            // Create new record for new flow:
            flowID = nextFlowID++;
            auto& flowID_vector = flowID_status->second;
            flowID_vector.insert(flowID_vector.begin(), flowID);

            auto newFlowRecord = flowRecords.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(flowID),
                                  std::forward_as_tuple(flowID, pktTime, FlowRecord::TIMESERIES));
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
              auto& flowID_vector = flowID_status->second;
              flowID_vector.insert(flowID_vector.begin(), flowID);

              auto newFlowRecord = flowRecords.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(flowID),
                                    std::forward_as_tuple(flowID, pktTime, FlowRecord::TIMESERIES));
              assert(newFlowRecord.second); // sanity check
              std::reference_wrapper<FlowRecord> flowR = newFlowRecord.first->second;
              flowPortReuse++;

              flowR.get().update(evalCxt);
              touchedFlows.insert(flowID);
              f_sim_insert(flowID, flowR.get().last());
            }
            else {
              debugLog << "FlowID " << flowID << " is no longer being tracked; "
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
                                std::forward_as_tuple(flowID, pktTime, FlowRecord::TIMESERIES));
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
            constexpr uint64_t RST_TIMEOUT = 10;
            constexpr uint64_t FIN_TIMEOUT = 60;
            constexpr uint64_t TCP_TIMEOUT = 600;
            constexpr uint64_t LONG_TIMEOUT = 120;

            ////////////////////////////////////////
            // FlowID reverse search lambda (linear)
            // - Needed when flows expire (no FKT from active packet)
            // FIXME: avoid linear search!
            auto find_key = [&flowIDs](flow_id_t id) -> const FlowKeyTuple {
              for (const auto& [fkt, fid_v] : flowIDs) {
                if (fid_v.front() == id) {
                  return fkt;
                }
              }
              return FlowKeyTuple();
            };

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
              debugLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                       << " considered terminated after RST, "
    //                     << (flowR.sawFIN()?"FIN, ":"RST, ")
                       << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
            else if (flowR.sawFIN() && sinceLast.tv_sec >= FIN_TIMEOUT) {
              debugLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                       << " considered terminated after FIN, "
    //                     << (flowR.sawFIN()?"FIN, ":"RST, ")
                       << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
            else if (flowR.isTCP() && sinceLast.tv_sec >= TCP_TIMEOUT) {
              debugLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                       << " considered dormant after " << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
              delete_flow(flowR_i);
            }
            else if (sinceLast.tv_sec >= LONG_TIMEOUT) {
              debugLog << (flowR.isUDP()?"UDP":"???") << " flow " << i
                       << " considered dormant afer " << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
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

  // Print Locality Simulation:
#ifdef ENABLE_SIM
  debugLog << "SimMIN Cache Size: " << simMIN.get_size() << '\n';
  debugLog << " - Hits: " << simMIN.get_hits() << '\n';
  debugLog << " - Miss (Compulsory): " << simMIN.get_compulsory_miss() << '\n';
  debugLog << " - Miss (Capacity): " << simMIN.get_capacity_miss() << '\n';
  debugLog << " - Max elements between barrier: " << simMIN.get_max_elements() << '\n';

  debugLog << "cacheSimLRU Cache Size: " << cacheSimLRU.get_size() << '\n';
  debugLog << " - Hits: " << cacheSimLRU.get_hits() << '\n';
  debugLog << " - Miss (Compulsory): " << cacheSimLRU.get_compulsory_miss() << '\n';
  debugLog << " - Miss (Capacity): " << cacheSimLRU.get_capacity_miss() << '\n';
#endif

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

  // Simulation finished, wait for uninstall from WSTP:
  {
    auto it = flowRecords.cbegin();
    while ( it != flowRecords.cend() ) {
      auto flowR = flowRecords.extract(it);
      f_sample( std::move(flowR.mapped()) );
      it = flowRecords.cbegin();
    }
  }

  for (auto& h : gz_handles) {
    h.flush();
  }

  cout << "Run took "
       << hrs << "h:" << min%60 << "m:"
       << sec%60 << "s:" << msec%1000 << "ms\n";
  cout << "Processed " << totalPackets << " packets ("
       << flowIDs.size() << " flows)." << endl;

  wstp.wait_for_unlink();
  cerr << "Main thread terminating." << endl;
  return EXIT_SUCCESS;
}
