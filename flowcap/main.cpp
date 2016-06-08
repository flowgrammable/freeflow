
#include <freeflow/time.hpp>
#include <freeflow/ip.hpp>
#include <freeflow/capture.hpp>

#include <sstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <cstring>


using namespace ff;


int usage(std::ostream&);
int error(char const*);
int help(int, char**);
int version(int, char**);
int dump(int, char**);
extern int forward(int, char**);
extern int replay(int, char**);
extern int expect(int, char**);
extern int fetch(int, char**);


int
main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "error: too few arguments\n\n";
    return usage(std::cerr);
  }

  // TODO: This is slow.
  std::string cmd = argv[1];
  if (cmd == "help")
    return help(argc, argv);
  else if (cmd == "version")
    return version(argc, argv);
  else if (cmd == "dump")
    return dump(argc, argv);
  else if (cmd == "forward")
    return forward(argc, argv);
  else if (cmd == "replay")
    return replay(argc, argv);
  else if (cmd == "expect")
    return expect(argc, argv);
  else if (cmd == "fetch")
    return fetch(argc, argv);
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
  os << "    flowcap replay <pcap-file> <hostname> <port>\n";
  os << "    flowcap expect <pcap-file> <hostname> <port>\n";
  os << "    flowcap fetch <pcap-file> <hostname> <port>\n";
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



