
#include <freeflow/capture.hpp>

#include <iostream>

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
  std::cout << "TEST: " << argv[2] << '\n';
  cap::Stream cap(cap::offline(argv[2]));

  // Iterate over each packet and print some basic information.
  int n = 0;
  cap::Packet p;
  while (cap.get(p)) {
    std::cout << "* " << p.size() << " bytes\n";
    ++n;
  }
  std::cout << "read " << n << " packets\n";

  return 0;
}


// TODO: Add options for IP versions, TCP and UDP.
int
forward(int argc, char* argv[])
{
  return 0;
}
