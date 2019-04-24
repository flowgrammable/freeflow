//#include "caida-pcap.hpp"

#include "util_caida.hpp"
#include "util_extract.hpp"
#include "util_bitmask.hpp"
#include "util_io.hpp"
#include "sim_min.hpp"
#include "sim_lru.hpp"

#include <string>
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

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"

// Wolfram Symbolic Transfer Protocol (WSTP):
#include "wstp_link.hpp"

using namespace std;
//using namespace util_view;
namespace po = boost::program_options;

using flow_id_t = u64;


static bool running;
void
sig_handle(int sig)
{
  running = false;
}


static std::ostream& print_eval(std::ostream &out, const EvalContext& evalCxt) {
  int committedBytes = evalCxt.v.absoluteBytes<int>() - evalCxt.v.committedBytes<int>();
  int pendingBytes = evalCxt.v.committedBytes<int>();
  string committed("|"), pending("|");
  if (committedBytes > 0) {
    committed = string(committedBytes * 2, '+');
  }
  if (pendingBytes > 0) {
    pending = string(pendingBytes * 2, '-');
    pending.append("|");
  }
  out << committed << pending << '\n';
#ifdef DEBUG_LOG
  out << evalCxt.extractLog.str();
#endif
  return out;
}


stringstream hexdump(const void* msg, size_t bytes) {
  auto m = static_cast<const uint8_t*>(msg);
  stringstream ss;

  ss << std::hex << std::setfill('0');
  for (decltype(bytes) i = 0; i < bytes; i++) {
    ss << setw(2) << static_cast<unsigned int>(m[i]);
  }
  return ss;
}


static void print_hex(const string& s) {
  stringstream ss = hexdump(s.c_str(), s.size());
  cerr << ss.str() << endl;
}


static stringstream dump_packet(const fp::Packet& p) {
  return hexdump(p.data(), p.size());
}


static void print_packet(const fp::Packet& p) {
  cerr << p.timestamp().tv_sec << ':' << p.timestamp().tv_nsec << '\n'
       << dump_packet(p).str() << endl;
}


static stringstream print_flow(const FlowRecord& flow) {
  stringstream ss;

  // Print flow summary information:
  auto pkts = flow.packets();
  auto byts = flow.bytes();
  ss << "FlowID=" << flow.getFlowID()
     << ", key=" << 0
     << ", packets=" << pkts
     << ", bytes=" << byts
     << ", ppBytes=" << byts/pkts
     << ", " << (flow.sawSYN()?"SYN":"")
             << (flow.sawFIN()?"|FIN":"")
             << (flow.sawRST()?"|RST\n":"\n");
  return ss;
}


static void print_log(const EvalContext& e) {
#ifdef DEBUG_LOG
  cerr << e.extractLog.str() << endl;
#endif
}


static void print_log(const FlowRecord& flow) {
  cerr << flow.getLog() << endl;
}


static void print_help(const char* argv0, const po::options_description desc) {
    cout << "Usage: " << argv0 << " [options] (directionA.pcap) [directionB.pcap]" << endl;
    cout << desc << endl;
}


///////////////////////////////////////////////////////////////////////////////
int
main(int argc, const char* argv[])
{
  signal(SIGINT, sig_handle);
  signal(SIGKILL, sig_handle);
  signal(SIGHUP, sig_handle);
  running = true;

  std::string flowsFilename;
  std::string traceFilename;
  std::string logfilename;
  std::string inputAfilename;
  std::string inputBfilename;

  caidaHandler caida;

  // Get current time (to generate trace filename):
  stringstream now_ss;
  chrono::system_clock::time_point start_time = chrono::system_clock::now();
  {
    time_t now_c = chrono::system_clock::to_time_t(start_time);
    now_ss << std::put_time(std::localtime(&now_c),"%F_%T");
  }


  // Boost commandline parsing:
  po::options_description nonpos_desc("Available options");
  po::options_description pos_desc("Positional options");
  po::options_description cmdline_desc("All options");
  po::positional_options_description boostPO;
  po::variables_map vm;

  nonpos_desc.add_options()
      ("help,h", "Display this message")
      ("log-file,l",
       po::value<std::string>(&logfilename)->default_value(now_ss.str().append(".log")),
       "The name of the log file")
      ("flows-file,f",
       po::value<std::string>(&flowsFilename)->default_value(now_ss.str().append(".flows")),
       "The name of the observed flows file")
      ("trace-file,t",
       po::value<std::string>(&traceFilename)->default_value(now_ss.str().append(".trace")),
       "The name of the timeseries trace file")
      ;

  pos_desc.add_options()
      ("directionA", po::value<std::string>(&inputAfilename), "The name of the pcap input file")
      ("directionB", po::value<std::string>(&inputBfilename), "The name of the pcap input file")
      ;

  boostPO.add("directionA", 1);
  boostPO.add("directionB", 1);

  cmdline_desc.add(nonpos_desc).add(pos_desc);

  try {
    po::store(po::command_line_parser(argc, argv)
              .options(cmdline_desc)
              .positional(boostPO)
              .run()
              , vm);

    if (vm.count("help")) {
      print_help(argv[0], nonpos_desc);
      return 0;
    }

    po::notify(vm);
  }
  catch (po::unknown_option ex) {
    cout << ex.what() << endl;
    print_help(argv[0], nonpos_desc);
    return 2;
  }

  // Ensure at least one input file is given
  if (!vm.count("directionA")) {
    cout << "An input file is required" << endl;
    print_help(argv[0], nonpos_desc);
    return 2;
  }

  // Open Log/Trace Output Files:
  ofstream debugLog;
  debugLog.open(logfilename, ios::out);

  // Open Pcap Input Files:
  debugLog << "Direction A: " << inputAfilename << '\n';
  caida.open(1, inputAfilename);
  if (vm.count("directionB")) {
    debugLog << "Direction B: " << inputBfilename << '\n';
    caida.open(2, inputBfilename);
  }

  // Output Files:
  // - unique_ptr to prevent resizing vector destroying gz_ostream.
  std::vector< unique_ptr<gz_ostream> > gz_handles;
  // boost gzip file open helper lambda:
  auto open_gzip = [&](const std::string& filename) -> ostream& {
    gz_handles.push_back( make_unique<gz_ostream>(filename) );
    return gz_handles.back()->get_ostream();
  };

  // Log output format:
  debugLog << "Logging to: " << logfilename << '\n';
  debugLog << "Tracing to: " << traceFilename << '\n';
  debugLog << "Stats dumping to: " << flowsFilename << '\n';

  constexpr auto FKT_SIZE = std::tuple_size<FlowKeyTuple>::value;
  debugLog << "File: " << flowsFilename << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n"
           << "- FlowID: " << sizeof(flow_id_t) << "B\n";
  debugLog << "File: " << traceFilename + ".gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n";


  // Flow Stats Output Files:
  auto& flowStats = open_gzip(flowsFilename + ".gz");
  debugLog << "File: " << flowsFilename + ".gz" << "\n";

  // Trace Output Files:
  auto& flowTrace = open_gzip(traceFilename + ".gz");
  debugLog << "File: " << traceFilename + ".gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n";
  auto& tcpFlowTrace = open_gzip(traceFilename + ".tcp.gz");
  debugLog << "File: " << traceFilename + ".tcp.gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n"
           << "- Flags: " << sizeof(Flags) << "B\n";
  auto& udpFlowTrace = open_gzip(traceFilename + ".udp.gz");
  debugLog << "File: " << traceFilename + ".udp.gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n";
  auto& otherFlowTrace = open_gzip(traceFilename + ".other.gz");
  debugLog << "File: " << traceFilename + ".other.gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n";
  auto& scanFlowTrace = open_gzip(traceFilename + ".scans.gz");
  debugLog << "File: " << traceFilename + ".scans.gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n"
           << "- Flags: " << sizeof(Flags) << "B\n";

  debugLog.flush();

  // Global Metadata structures:
  flow_id_t nextFlowID = 1; // flowID 0 is reserved for 'flow not found'
  absl::flat_hash_map<FlowKeyTuple, absl::InlinedVector<flow_id_t,1>> flowIDs;
    // maps flows to vector (from most-recent to least-recent) of flowIDs
  std::map<flow_id_t, FlowRecord> flowRecords;
  std::unordered_set<flow_id_t> blacklistFlows;
  std::set<flow_id_t> touchedFlows;

  // Retired Records:
  std::map<flow_id_t, const FlowRecord> retiredRecords;
  std::mutex mtx_RetiredFlows;

  // Simulation bookeeping structures:
#define ENABLE_SIM
#ifdef ENABLE_SIM
  SimMIN<flow_id_t> simMIN(1<<16);  // 64k
  SimLRU<flow_id_t> simLRU(1<<16);
  std::mutex mtx_Misses; // covers both missesMIN and missesLRU
  std::map<flow_id_t, std::vector<uint64_t>> missesMIN;
  std::map<flow_id_t, std::vector<uint64_t>> missesLRU;
#endif

  auto f_sort_by_misses = [&](const std::map<flow_id_t, std::vector<uint64_t>>& misses) {
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

  // Define functions to interact with replacement:
  std::function<void(flow_id_t, const std::pair<uint64_t,timespec>&&)> f_sim_insert =
      [&](flow_id_t flowID, const std::pair<uint64_t,timespec>&& pktTime) {
#ifdef ENABLE_SIM
    auto [ns, ts] = pktTime;
    simMIN.insert(flowID, ts);
    simLRU.insert(flowID, ts);
    {
      std::lock_guard lock(mtx_Misses);
      missesMIN.erase(flowID);
      missesLRU.erase(flowID);
    }

#endif
  };

  std::function<void(flow_id_t, const std::pair<uint64_t,timespec>&&)> f_sim_update =
      [&](flow_id_t flowID, const std::pair<uint64_t,timespec>&& pktTime) {
#ifdef ENABLE_SIM
    auto [ns, ts] = pktTime;
    if (!simMIN.update(flowID, ts)) {
      std::lock_guard lock(mtx_Misses);
      missesMIN[flowID].emplace_back(ns);
    }
    if (!simLRU.update(flowID, ts)) {
      std::lock_guard lock(mtx_Misses);
      missesLRU[flowID].emplace_back(ns);
    }
#endif
  };

  wstp_link::fn_t f_get_misses_MIN = [&](flow_id_t) {
    static size_t update_triger = retiredRecords.size();
    static auto sorted_queue = f_sort_by_misses(missesMIN);
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
    if (record.empty()) {
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


  // Global Stats:
  u16 maxPacketSize = std::numeric_limits<u16>::lowest();
  u16 minPacketSize = std::numeric_limits<u16>::max();
  u64 totalPackets = 0, totalBytes = 0, \
      blacklistPackets = 0, malformedPackets = 0, timeoutPackets = 0, \
      flowPortReuse = 0;

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

  std::function<void(FlowRecord&&)> f_sample = [&](FlowRecord&& r) {
    // Only consider semi-large flows:
    if (r.packets() > 512) {
//      auto lru_it = missesLRU.find(r.getFlowID());
      auto min_it = missesMIN.find(r.getFlowID());
      const auto& min_ts = min_it->second;
      // Only record multiple misses within MIN:
      if ( min_it != missesLRU.end() && min_ts.size() >= 8 ) {
        // Flow looks interesting, add to retiredRecords:
        std::lock_guard lock(mtx_RetiredFlows);
        retiredRecords.emplace(std::make_pair(r.getFlowID(), r));
      }
      else {
        // Flow behaved unremarkibly, delete metadata:
        std::lock_guard lck(mtx_Misses);
        missesLRU.erase(r.getFlowID());
        missesMIN.erase(r.getFlowID());
      }
    }
  };

  // Instantiate Mathematica Link:
  wstp_link wstp(1, argv);
  wstp.log( now_ss.str().append(".wstp.log") );
  vector<wstp_link::def_t> definitions = {
    make_tuple(f_get_arrival, "FFSampleFlow[]", ""),
    make_tuple(f_get_misses_MIN, "FFGetMisses[]", ""),
    make_tuple(f_num_flows, "FFNumFlows[]", "")
  };
  wstp.install(definitions);


  ///////////////////////
  // PCAP file read loop:
  s64 count = 0, last_count = 0;
  fp::Context cxt = caida.recv();
  timespec last_epoc = cxt.packet().timestamp();
  while (running && cxt.packet_ != nullptr) {
    flow_id_t flowID;
    const timespec& pktTime = cxt.packet().timestamp();

    { // per-packet processing
      const fp::Packet& p = cxt.packet();
//      evalQueue.emplace(p);
//      EvalContext& evalCxt = evalQueue.back();
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

            // FlowID reverse search lambda (linear):
            // FIXME: avoid linear search!
            auto find_key = [&flowIDs](flow_id_t id) -> const FlowKeyTuple {
              for (const auto& [fkt, fid_v] : flowIDs) {
                if (fid_v.front() == id) {
                  return fkt;
                }
              }
              return FlowKeyTuple();
            };

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
//              flowRecords.erase(flowR_i);
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
//              flowRecords.erase(flowR_i);
              delete_flow(flowR_i);
            }
            else if (flowR.isTCP() && sinceLast.tv_sec >= TCP_TIMEOUT) {
              debugLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                       << " considered dormant after " << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
//              flowRecords.erase(flowR_i);
              delete_flow(flowR_i);
            }
            else if (sinceLast.tv_sec >= LONG_TIMEOUT) {
              debugLog << (flowR.isUDP()?"UDP":"???") << " flow " << i
                       << " considered dormant afer " << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
//              flowRecords.erase(flowR_i);
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
  debugLog << " - Miss (Compulsory): " << simMIN.get_compulsory_miss() << '\n';
  debugLog << " - Hits: " << simMIN.get_hits() << '\n';
  debugLog << " - Miss (Capacity): " << simMIN.get_capacity_miss() << '\n';
  debugLog << " - Max elements between barrier: " << simMIN.get_max_elements() << '\n';
  debugLog << "SimLRU Cache Size: " << simLRU.get_size() << '\n';
  debugLog << " - Miss (Compulsory): " << simLRU.get_compulsory_miss() << '\n';
  debugLog << " - Hits: " << simLRU.get_hits() << '\n';
  debugLog << " - Miss (Capacity): " << simLRU.get_capacity_miss() << '\n';
#endif

  chrono::system_clock::time_point end_time = chrono::system_clock::now();
  auto msec = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
  auto sec  = msec/1000;
  auto min  = sec/60;
  auto hrs  = min/60;
  debugLog << "Run took "
           << hrs << "h:" << min%60 << "m:"
           << sec%60 << "s:" << msec%1000 << "ms" << endl;
//  debugLog << "Capture Start: " << packet_time << '\n'
//           << "Capture End: " << packet_time2 << '\n'
//           << "Capture Duration: " << packet_time2-packet_time << endl;
  cout << "Run took "
       << hrs << "h:" << min%60 << "m:"
       << sec%60 << "s:" << msec%1000 << "ms\n";
  cout << "Processed " << totalPackets << " packets ("
       << flowIDs.size() << " flows)." << endl;

  // Simulation finished, wait for uninstall from WSTP:
  {
    auto it = flowRecords.cbegin();
    while ( it != flowRecords.cend() ) {
      auto flowR = flowRecords.extract(it);
      f_sample( std::move(flowR.mapped()) );
      it = flowRecords.cbegin();
    }
  }

  wstp.wait();
  return 0;
}
