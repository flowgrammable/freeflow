
#include <freeflow/time.hpp>
#include <freeflow/ip.hpp>
#include <freeflow/capture.hpp>

#include <sstream>
#include <iostream>
#include <iomanip>
#include <locale>


using namespace ff;


int usage(std::ostream&);
int error(char const*);
int help(int, char**);
int version(int, char**);
int dump(int, char**);
int forward(int, char**);


int
main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "error: too few arguments\n\n";
    return usage(std::cerr);
  }

  std::string cmd = argv[1];
  if (cmd == "help")
    return help(argc, argv);
  else if (cmd == "version")
    return version(argc, argv);
  else if (cmd == "dump")
    return dump(argc, argv);
  else if (cmd == "forward")
    return forward(argc, argv);
  else
    return error(argv[1]);
}


// Returns 1 if the output stream is cerr, 0 otherwise.
int
usage(std::ostream& os)
{
  os << "usage\n";
  os << "    flowcap help [topic]\n";
  os << "    flowcap version\n";
  os << "    flowcap dump <pcap-file>\n";
  os << "    flowcap forward <pcap-file> <hostname> <port>\n";
  return &os == &std::cerr;
}


int
error(char const* cmd)
{
  std::cerr << "invalid command '" << cmd << "'\n";
  return usage(std::cerr);
}


int
help(int argc, char* argv[])
{
  version(argc, argv);
  std::cout << '\n';
  return usage(std::cout);
}


int
version(int argc, char* argv[])
{
  // FIXME: Show copyrights, etc.
  std::cout << "flowcap 1.0\n";
  return 0;
}


int
dump(int argc, char* argv[])
{
  if (argc < 3) {
    std::cerr << "error: too few arguments to 'dump'\n";
    return usage(std::cerr);
  }

  // Open an offline stream capture.
  cap::Stream cap(cap::offline(argv[2]));

  // Iterate over each packet and print some basic information.
  int n = 0;
  cap::Packet p;
  while (cap.get(p)) {
    std::cout << "* " << p.captured_size() << " bytes\n";
    ++n;
  }
  std::cout << "read " << n << " packets\n";

  return 0;
}


// TODO: Add options for IP versions, TCP and UDP.
int
forward(int argc, char* argv[])
{
  if (argc < 5) {
    std::cerr << "error: too few arguments to 'dump'\n";
    return usage(std::cerr);
  }

  std::string path = argv[2];
  std::string host = argv[3];
  std::string port = argv[4];

  // Convert the host name to an address.
  Ipv4_address addr;
  try {
    addr = host;
  } catch (std::runtime_error& err) {
    std::cerr << "error: " << err.what() << '\n';
    return 1;
  }

  // Convert the port number.
  std::uint16_t portnum;
  std::stringstream ss(port);
  ss >> portnum;
  if (ss.fail() && !ss.eof()) {
    std::cerr << "error: invalid port '" << port << "'\n";
    return 1;
  }


  // Build and connect.
  Ipv4_stream_socket sock;
  if (!sock.connect({addr, portnum})) {
    std::cerr << "error: could not connect to host\n";
    return -1;
  }

  // Open an offline stream capture.
  cap::Stream cap(cap::offline(argv[2]));

  // Iterate over each packet and and send each packet to the
  // connected host.
  Time start = now();
  std::uint64_t n = 0;
  std::uint64_t b = 0;
  cap::Packet p;
  while (cap.get(p)) {
    int k = sock.send(p.data(), p.captured_size());
    if (k <= 0) {
      if (k < 0) {
        std::cerr << "error: " << std::strerror(errno) << '\n';
        return 1;
      }
      return 0;
    }
    // std::cout << "sent: " << p.captured_size() << " bytes\n";
    ++n;
    b += p.captured_size();
  }
  Time stop = now();

  // Make some measurements.
  Fp_seconds dur = stop - start;
  double s = dur.count();
  // double Gb = double(b * 8) / (1 << 30);
  double Mb = double(b * 8) / (1 << 20);
  // double Kb = double(b * 8) / (1 << 10);
  // double Gbps = Gb / s;
  double Mbps = Mb / s;
  // double Kbps = Kb / s;
  double Pps = n / s;

  std::cout.imbue(std::locale(""));
  std::cout << "sent " << n << " packets in " << s << " seconds (" << Pps << " Pps)\n";
  // std::cout << "average " << double(b) / n << " Bpp\n";

  std::cout.precision(6);
  std::cout << "sent " << b << " bytes";

  // if (Gb > 1)
  // else if (Mb > 1)
  //   std::cout << "sent " << Mb << " Mb";
  // else
  //   std::cout << "sent " << Kb << " Kb";

  std::cout.precision(1);
  std::cout << " in " << s << " (";

  std::cout.precision(6);
  std::cout << Mbps << " Mbps)\n";

  // if (Gbps > 1)
  //   std::cout << Gbps << " Gbps)\n";
  // else if (Mbps > 1)
  //   std::cout << Mbps << " Mbps)\n";
  // else
  //   std::cout << Kbps << " Kbps)\n";

  return 0;
}
