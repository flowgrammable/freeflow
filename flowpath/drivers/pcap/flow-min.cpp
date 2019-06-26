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

  // Simulation Eval structures:
  SimMIN<flow_id_t> simMIN(1<<16);  // 64k
  SimLRU<flow_id_t> simLRU(1<<16);

  // Global Stats:
  u16 maxPacketSize = std::numeric_limits<u16>::lowest();
  u16 minPacketSize = std::numeric_limits<u16>::max();
  u64 totalPackets = 0, totalBytes = 0, \
      blacklistPackets = 0, malformedPackets = 0, timeoutPackets = 0, \
      flowPortReuse = 0;

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
                                  std::forward_as_tuple(flowID, pktTime));
            assert(newFlowRecord.second); // sanity check
            flowR = newFlowRecord.first->second;
            flowPortReuse++;
            simMIN.insert(flowID, pktTime);
            simLRU.insert(flowID, pktTime);
          }
          else {
            simMIN.update(flowID, pktTime);
            simLRU.update(flowID, pktTime);
          }

          // Flow is currently tracked:
          flowR.get().update(evalCxt);
          touchedFlows.insert(flowID);
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
                                    std::forward_as_tuple(flowID, pktTime));
              assert(newFlowRecord.second); // sanity check
              std::reference_wrapper<FlowRecord> flowR = newFlowRecord.first->second;
              flowPortReuse++;

              flowR.get().update(evalCxt);
              touchedFlows.insert(flowID);
              simMIN.insert(flowID, pktTime);
              simLRU.insert(flowID, pktTime);
            }
            else {
              debugLog << "FlowID " << flowID << " is no longer being tracked; "
                       << print_flow_key_string(k) << endl;
              timeoutPackets++;
              simMIN.update(flowID, pktTime); // TODO, is this broken?
              simLRU.update(flowID, pktTime);
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
                                std::forward_as_tuple(flowID, pktTime));
          assert(newFlowRecord.second); // sanity check
          newFlowRecord.first->second.update(evalCxt);
          simMIN.insert(flowID, pktTime);
          simLRU.insert(flowID, pktTime);
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
              flowRecords.erase(flowR_i);
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
              flowRecords.erase(flowR_i);
            }
            else if (flowR.isTCP() && sinceLast.tv_sec >= TCP_TIMEOUT) {
              debugLog << (flowR.isTCP()?"TCP":"???") << " flow " << i
                       << " considered dormant after " << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
              flowRecords.erase(flowR_i);
            }
            else if (sinceLast.tv_sec >= LONG_TIMEOUT) {
              debugLog << (flowR.isUDP()?"UDP":"???") << " flow " << i
                       << " considered dormant afer " << flowR.packets()
                       << " packets and " << sinceLast.tv_sec
                       << " seconds of inactivity; "
                       << print_flow_key_string( find_key(i) ) << endl;
              flowStats << print_flow(flowR).str();
              flowRecords.erase(flowR_i);
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
  debugLog << "SimMIN Cache Size: " << simMIN.get_size() << '\n';
  debugLog << " - Miss (Compulsory): " << simMIN.get_compulsory_miss() << '\n';
  debugLog << " - Hits: " << simMIN.get_hits() << '\n';
  debugLog << " - Miss (Capacity): " << simMIN.get_capacity_miss() << '\n';
  debugLog << " - Max elements between barrier: " << simMIN.get_max_elements() << '\n';
  debugLog << "SimLRU Cache Size: " << simLRU.get_size() << '\n';
  debugLog << " - Miss (Compulsory): " << simLRU.get_compulsory_miss() << '\n';
  debugLog << " - Hits: " << simLRU.get_hits() << '\n';
  debugLog << " - Miss (Capacity): " << simLRU.get_capacity_miss() << '\n';


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

  return 0;
}

