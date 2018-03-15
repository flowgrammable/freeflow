#include "system.hpp"
#include "dataplane.hpp"
#include "application.hpp"
#include "port_table.hpp"
#include "port.hpp"
#include "port_odp.hpp"
#include "context.hpp"
///#include "packet.hpp" // temporary for sizeof packet
///#include "context.hpp" // temporary for sizeof context

#include <string>
#include <iostream>
#include <signal.h>

#include <iostream>
#include <pcap.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

using namespace std;

static bool running;

void packetHandler(u_char *userData, const struct pcap_pkthdr* pkthdr, const u_char* packet);

void
sig_handle(int sig)
{
  running = false;
}

int
main(int argc, const char* argv[])
{
  signal(SIGINT, sig_handle);
  signal(SIGKILL, sig_handle);
  signal(SIGHUP, sig_handle);
  running = true;
  try
  {
  pcap_t *descr;

  // open capture file for offline processing
  {
    char errbuf[PCAP_ERRBUF_SIZE];
    descr = pcap_open_offline("http.cap", errbuf);
    if (descr == NULL) {
        cout << "pcap_open_live() failed: " << errbuf << endl;
        return 1;
    }
  }

  // start packet processing loop, just like live capture
  if (pcap_loop(descr, 0, packetHandler, NULL) < 0) {
      cout << "pcap_loop() failed: " << pcap_geterr(descr);
      return 1;
  }

  cout << "capture finished" << endl;


//// Flowpath Wire Example ////
    // Instantiate ports.
    //
    // P1 : Connected to an echo client.
    //fp::Port* p1 = fp::create_port(fp::Port::Type::odp_burst, "veth1;p1");
//    fp::Port* p1 = fp::create_port(fp::Port::Type::odp_burst, "0;p1");
    fp::Port* p1 = fp::port_table.alloc(fp::Port::Type::odp_burst, "0;p1");
    std::cerr << "Created port " << p1->name() << " with id '" << p1->id() << std::endl;

    // P2 : Bound to a netcat TCP port; Acts as the entry point.
    //fp::Port* p2 = fp::create_port(fp::Port::Type::odp_burst, "veth3;p2");
//    fp::Port* p2 = fp::create_port(fp::Port::Type::odp_burst, "1;p2");
    fp::Port* p2 = fp::port_table.alloc(fp::Port::Type::odp_burst, "1;p2");
    std::cerr << "Created port " << p2->name() << " with id '" << p2->id() << std::endl;

    // Load the application library
    const char* appPath = "apps/wire.app";
    if (argc == 2)
      appPath = argv[1];

//    fp::load_application(appPath);
//    std::cerr << "Loaded application " << appPath << std::endl;

    // Create the dataplane with the loaded application library.
//    fp::Dataplane* dp = fp::create_dataplane("dp1", appPath);
    fp::Dataplane dp("dp1");
    std::cerr << "Created data plane " << dp.name() << std::endl;

    // Configure the data plane based on the applications needs.
    ///dp->configure();
//    std::cerr << "Data plane configured." << std::endl;

    // Add all ports
    dp.add_virtual_ports();
    dp.add_port(p1);
    std::cerr << "Added port 'p1' to data plane 'dp1'" << std::endl;
    dp.add_port(p2);
    std::cerr << "Added port 'p2' to data plane 'dp1'" << std::endl;

    dp.load_application(appPath);
    std::cerr << "Loaded application " << appPath << std::endl;

    // Start the dataplane.
    dp.up(); // should this up ports?
    p1->open();
    p1->up();
    p2->open();
    p2->up();
    std::cerr << "Data plane 'dp1' up" << std::endl;

    // Loop forever.
    // - currently recv() automatically runs packet through pipeline.
    while (running) {
//      // FIXME: Hack to recieve on both ports until new port_table interface is defined...
//      int pkts = 0;
//      pkts = p1->recv();
//      pkts = p1->send();

//      pkts = p2->recv();
//      pkts = p2->send();

      // simple round robin send/recv for a single application
      fp::Application* app = dp.get_application();
      for (fp::Port* p : dp.ports()) {
        // Allocate context on stack
        fp::Context cxt;
        int pkts = p->recv(&cxt);
        if (pkts > 0) {
          app->process(cxt);

          if (fp::Port* out = dp.get_port(cxt.output_port())) {
            out->send(&cxt);
          }
        }
      }
    };

    // Stop the wire.
    dp.down();
    std::cerr << "Data plane 'dp1' down" << std::endl;

    // TODO: Report some statistics?
    // dp->app()->statistics(); ?

    // Cleanup
//    fp::delete_port(p1->id());
//    std::cerr << "Deleted port 'p1' with id '" << p1->id() << std::endl;
//    fp::delete_port(p2->id());
//    std::cerr << "Deleted port 'p2' with id '" << p1->id() << std::endl;
//    fp::delete_dataplane("dp1");

//    fp::unload_application("apps/wire.app");
    dp.unload_application();
    std::cerr << "Unloaded application 'apps/wire.app'" << std::endl;
  }
  catch (std::string msg)
  {
    std::cerr << msg << std::endl;
    return -1;
  }
  return 0;
}


void packetHandler(u_char *userData, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
  const struct ether_header* ethernetHeader;
  const struct ip* ipHeader;
  const struct tcphdr* tcpHeader;
  char sourceIp[INET_ADDRSTRLEN];
  char destIp[INET_ADDRSTRLEN];
  u_int sourcePort, destPort;
  u_char *data;
  int dataLength = 0;
  string dataStr = "";

  ethernetHeader = (struct ether_header*)packet;
  if (ntohs(ethernetHeader->ether_type) == ETHERTYPE_IP) {
      ipHeader = (struct ip*)(packet + sizeof(struct ether_header));
      inet_ntop(AF_INET, &(ipHeader->ip_src), sourceIp, INET_ADDRSTRLEN);
      inet_ntop(AF_INET, &(ipHeader->ip_dst), destIp, INET_ADDRSTRLEN);

      if (ipHeader->ip_p == IPPROTO_TCP) {
          tcpHeader = (tcphdr*)(packet + sizeof(struct ether_header) + sizeof(struct ip));
          sourcePort = ntohs(tcpHeader->source);
          destPort = ntohs(tcpHeader->dest);
          data = (u_char*)(packet + sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct tcphdr));
          dataLength = pkthdr->len - (sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct tcphdr));

          // convert non-printable characters, other than carriage return, line feed,
          // or tab into periods when displayed.
          for (int i = 0; i < dataLength; i++) {
              if ((data[i] >= 32 && data[i] <= 126) || data[i] == 10 || data[i] == 11 || data[i] == 13) {
                  dataStr += (char)data[i];
              } else {
                  dataStr += ".";
              }
          }

          // print the results
          cout << sourceIp << ":" << sourcePort << " -> " << destIp << ":" << destPort << endl;
          if (dataLength > 0) {
              cout << dataStr << endl;
          }
      }
  }
}

