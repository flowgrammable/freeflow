
#include <freeflow/time.hpp>
#include <freeflow/ip.hpp>
#include <freeflow/capture.hpp>

#include <sstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <cstring>


using namespace ff;


extern int usage(std::ostream&);


// TODO: Add options for IP versions, TCP and UDP.
int
forward(int argc, char* argv[])
{
  if (argc < 5) {
    std::cerr << "error: too few arguments to 'forward'\n";
    return usage(std::cerr);
  }

  std::string path = argv[2];
  std::string host = argv[3];
  std::string port = argv[4];

  int iterations = 1;
  if (argc > 5)
    iterations = std::stoi(argv[5]);

  // Convert the host name to an address.
  Ipv4_address addr;
  try {
    addr = host;
  } 
  catch (std::runtime_error& err) {
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
  if (cap.link_type() != cap::ethernet_link) {
    std::cerr << "error: input is not ethernet\n";
    return 1;
  }

  // Iterate over each packet and send each packet to the
  // connected host.
  std::uint64_t n = 0;
  std::uint64_t b = 0;
  cap::Packet p;

  Time start = now();
  while (cap.get(p)) {
    std::uint8_t buf[4096];
    std::size_t len = p.captured_size();
    std::uint32_t hdr = htonl(len);
    std::memcpy(&buf[0], &hdr, 4);
    std::memcpy(&buf[4], p.data(), len);

    int k = sock.send(buf, len + 4);
    if (k <= 0) {
      if (k < 0) {
        std::cerr << "send error: " << std::strerror(errno) << '\n';
        return 1;
      }
      break;
    }
    ++n;
    b += len;
  }
  Time stop = now();
  
  // Make some measurements.
  Fp_seconds dur = stop - start;
  double s = dur.count();
  double Mb = double(b * 8) / (1 << 20);
  double Mbps = Mb / s;
  std::uint64_t Pps = n / s;

  // FIXME: Make this pretty.
  // std::cout.imbue(std::locale(""));
  std::cout << "sent " << n << " packets in " 
            << s << " seconds (" << Pps << " Pps)\n";
  std::cout << "sent " << b << " bytes in " 
            << s << " seconds (" << Mbps << " Mbps)\n";

  return 0;
}
