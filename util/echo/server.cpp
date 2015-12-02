#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>

const int MAX_PACKET_SIZE = 1522;
int const HELLO = 0xAAAA;
int const GOODBYE = 0x5555;

struct Socket
{
  using Buffer = char[MAX_PACKET_SIZE];
  using Clients = std::vector<int>;
  using Address = struct sockaddr_in;

  Socket(std::string const& binding, int port)
    : port_(port), listen_(socket(AF_INET, SOCK_DGRAM, 0))
  {
    std::memset((struct sockaddr_in*)&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htons(INADDR_ANY);
    addr_.sin_port = htons(port);
  }

  inline void bind()
  {
    ::bind(listen_, (struct sockaddr*) &addr_, sizeof(addr_));
  }

  inline void listen()
  {
    ::listen(listen_, 1000);
    recv_ = accept(listen_, (struct sockaddr*) NULL, NULL);
  }

  inline void clear_buff()
  {
    std::memset(buf_, 0, MAX_PACKET_SIZE);
  }

  inline void recv()
  {
    read(recv_, buf_, MAX_PACKET_SIZE);
    if (std::memcmp(buf_, &HELLO, 2) == 0)
      clients_.push_back(recv_);
    else if (std::memcmp(buf_, &GOODBYE, 2) == 0)
      clients_.erase(std::find(clients_.begin(), clients_.end(), recv_));
  }

  inline void send()
  {
    for (int dest : clients_)
      write(dest, buf_, strlen(buf_) + 1);
  }


  Address addr_;
  int     port_;
  Buffer  buf_;
  int     listen_;
  int     recv_;
  Clients clients_;
};

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Invalid arguments. Usage: " << argv[0] <<
      " [port]\n";
    return -1;
  }

  // Create the socket on the port.
  Socket sock("", atoi(argv[1]));
  // Bind.
  sock.bind();
  // Listen.
  sock.listen();

  // Start processing loop.
  while(1)
  {
    sock.recv();
    sock.send();
  }

  // Cleanup.

  return 0;
}
