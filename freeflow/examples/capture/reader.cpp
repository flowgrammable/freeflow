
#include <freeflow/capture.hpp>

#include <iostream>

using namespace ff;

int main(int argc, char* argv[])
{
  if (argc != 2) {
    std::cerr << "error: too few arguments\n\n";
    std::cerr << "usage: " << argv[0] << " <pcap-file>\n";
    return -1;
  }

  // Open an offline stream capture.
  cap::Stream cap(cap::offline(argv[1]));
  

  // Iterate over each packet and print some basic information.
  cap::Packet p;
  while (cap.get(p))
    std::cout << "* packet with " << p.size() << '\n';

  return 0;
}
