
#include <freeflow/time.hpp>
#include <freeflow/ip.hpp>
#include <freeflow/capture.hpp>

#include <cassert>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <cstring>


using namespace ff;


extern int usage(std::ostream&);


// Fetch works like expect, except that it connects to the specified 
// source instead of accepting a connection. 
//
// TODO: This could legitimately be part of the expect framework, but
// using options to distinguish between passive and active connections.
int
fetch(int argc, char* argv[])
{
  // FIXME: We should be using our own usage function, not the general
  // application's.
  if (argc < 5) {
    std::cerr << "error: too few arguments to 'expect'\n";
    return usage(std::cerr);
  }

  std::string path = argv[2];
  std::string host = argv[3];
  std::string port = argv[4];

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

  // Build and connect the socket.
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

  // Iterate over each packet and and send each packet to the
  // connected host.
  Time start = now();
  std::uint64_t n = 0;
  std::uint64_t b = 0;
  cap::Packet p;
  while (cap.get(p)) {
    // FIXME: This is broken. We know exactly how many bytes are expected 
    // in a packet. We should receive exactly that many.
    char buf[4096];
    assert(p.captured_size() < 4096);
    int k = sock.recv(buf, p.captured_size() + 4);
    // int k = sock.recv(buf, p.captured_size());
    if (k <= 0) {
      if (k < 0) {
        std::cerr << "error: " << std::strerror(errno) << '\n';
        return 1;
      }
      return 0;
    }
    // assert(k == p.captured_size() + 4);

    // TODO: Verify that the content captured actually matches the content
    // sent. That seems like a good idea.

    ++n;
    b += p.captured_size();
  }
  Time stop = now();

  // Make some measurements.
  Fp_seconds dur = stop - start;
  double s = dur.count();
  double Mb = double(b * 8) / (1 << 20);
  double Mbps = Mb / s;
  double Pps = n / s;

  // FIXME: Make this pretty.
  std::cout.imbue(std::locale(""));
  std::cout << "received " << n << " packets in " 
            << s << " seconds (" << Pps << " Pps)\n";
  std::cout << "received " << b << " bytes in " 
            << s << " seconds (" << Mbps << " Mbps)\n";

  return 0;
}
