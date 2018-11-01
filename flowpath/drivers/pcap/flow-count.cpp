//#include "caida-pcap.hpp"

#include "util_caida.hpp"
#include "util_extract.hpp"
#include "util_bitmask.hpp"
#include "util_io.hpp"

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


static stringstream dump_packet(fp::Packet* p) {
  return hexdump(p->data(), p->size());
}


static void print_packet(fp::Packet* p) {
  cerr << p->timestamp().tv_sec << ':' << p->timestamp().tv_nsec << '\n'
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
     << '\n';
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
  po::positional_options_description p;
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

  p.add("directionA", 1);
  p.add("directionB", 1);

  cmdline_desc.add(nonpos_desc).add(pos_desc);

  try {
    po::store(po::command_line_parser(argc, argv)
              .options(cmdline_desc)
              .positional(p)
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

  constexpr auto FKT_SIZE = sizeof_tuple(FlowKeyTuple{});
  debugLog << "File: " << flowsFilename << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n"
           << "- FlowID: " << sizeof(flow_id_t) << "B\n";
  debugLog << "File: " << traceFilename + ".gz" << "\n"
           << "- Flow Key String: " << FKT_SIZE << "B\n";


  // Flow Stats Output Files:
  auto& flowStats = open_gzip(flowsFilename + ".gz");

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

  debugLog.flush();

  // Metadata structures:
  flow_id_t nextFlowID = 1; // flowID 0 is reserved for 'flow not found'
  absl::flat_hash_map<FlowKeyTuple, flow_id_t> flowIDs;
  std::map<flow_id_t, FlowRecord> flowRecords;

  // Global Stats:
  u16 maxPacketSize = std::numeric_limits<u16>::lowest();
  u16 minPacketSize = std::numeric_limits<u16>::max();
  u64 totalPackets = 0, totalBytes = 0;


  // PCAP file read loop:
  s64 count = 0;
  fp::Context cxt = caida.recv();
  while (cxt.packet_ != nullptr) {
    fp::Packet* p = cxt.packet_;
    EvalContext evalCxt(p);
    const auto& time = p->timestamp();

    // Attempt to parse packet headers:
    count++;
    try {
      int status = extract(evalCxt, IP);
    }
    catch (std::exception& e) {
      stringstream ss;
      ss << count << " (" << p->size() << "B): ";
      ss << evalCxt.v.absoluteBytes<int>() << ", "
         << evalCxt.v.committedBytes<int>() << ", "
         << evalCxt.v.pendingBytes<int>() << '\n';
      ss << dump_packet(p).str() << '\n';
      print_eval(ss, evalCxt);
      ss << "> Exception occured during extract: " << e.what();
//      debugLog <<  ss.str() << '\n';
      cerr << ss.str() << endl;
    }

    // Associate packet with flow:
    const Fields& k = evalCxt.fields;
    const FlowKeyTuple fkt = make_flow_key_tuple(k);

    u16 wireBytes = evalCxt.origBytes;
    maxPacketSize = std::max(maxPacketSize, wireBytes);
    minPacketSize = std::min(minPacketSize, wireBytes);
    totalBytes += wireBytes;
    totalPackets++;

    // Perform initial flowID lookup:
    flow_id_t flowID;
    {
      auto status = flowIDs.find(fkt);
      flowID = (status != flowIDs.end()) ? status->second : 0;
    }

    auto tracePoint = [&]() {
      const auto& fields = evalCxt.fields;
      const auto& proto = fields.fProto;

      // if flow is established?...

      if (proto & ProtoFlags::isTCP) {
        serialize(tcpFlowTrace, fkt);
        serialize(tcpFlowTrace, evalCxt.fields.fTCP);
      }
      else if (proto & ProtoFlags::isUDP) {
        serialize(udpFlowTrace, fkt);
      }
      else {
        serialize(flowTrace, fkt);
      }
    };

    // If flow exists:
    if (flowID != 0) {
      // Known Flow:
      tracePoint();
    }
    else {
      // Unkown Flow:
      auto newFlowID = flowIDs.emplace(std::piecewise_construct,
                            std::forward_as_tuple(fkt),
                            std::forward_as_tuple(nextFlowID));
      assert(newFlowID.second); // sanity check

      auto newFlowRecord = flowRecords.emplace(std::piecewise_construct,
                            std::forward_as_tuple(nextFlowID),
                            std::forward_as_tuple(nextFlowID, time));
      assert(newFlowRecord.second); // sanity check
      newFlowRecord.first->second.update(evalCxt);

      tracePoint();
      flowID = nextFlowID++;
    }

    // Release resources and retrieve another packet:
    caida.rebound(&cxt);
    cxt = caida.recv();
  }

  // Print flow tuples:
  for (const auto& flow : flowRecords) {
    const auto& fk = flow.first;
    const FlowRecord& fr = flow.second;

    serialize(flowStats, fk);
    serialize(flowStats, fr.getFlowID());
    serialize(flowStats, fr.packets());
    // output other flow statistics?
  }


  // Print global stats:
  debugLog << "Max packet size: " << maxPacketSize << '\n';
  debugLog << "Min packet size: " << minPacketSize << '\n';
  debugLog << "Total bytes: " << totalBytes << '\n';
  debugLog << "Total packets: " << totalPackets << '\n';

  chrono::system_clock::time_point end_time = chrono::system_clock::now();
  auto msec = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
  auto sec  = msec/1000;
  auto min  = sec/60;
  auto hrs  = min/60;
  cout << "Run took "
       << hrs << "h:" << min%60 << "m:"
       << sec%60 << "s:" << msec%1000 << "ms" << endl;

  return 0;
}

