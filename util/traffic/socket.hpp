#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <iostream>

const int MAX_PACKET_SIZE = 128;
int const HELLO = 0xAAAA;
int const GOODBYE = 0x5555;

// Base for a generic socket. Semantics for the functionality needed to actually
// use the socket is implementation defined for the type of socket being used.
struct Socket
{
  // The underlying buffer used for I/O.
  using Buffer = char[MAX_PACKET_SIZE];
  // The address of the socket.
  using Address = struct sockaddr_in;

  // Ctor/vDtor.
  Socket() 
  { 
    memset(buf_, 0, MAX_PACKET_SIZE);
  }
  virtual ~Socket() { }

  // Socket state functions.
  virtual void open() = 0;
  virtual void close() = 0;

  // Socket I/O functions.
  virtual bool recv() = 0;
  virtual bool send() = 0;

  // Zeros out the buffer.
  inline void clear_buff() { std::memset(buf_, 0, MAX_PACKET_SIZE); }

  // The sockets address.
  Address   addr_;
  // Address length, for convenience.
  socklen_t addr_len_ = sizeof(addr_);
  // I/O buffer.
  Buffer    buf_;
  // File descriptor for socket.
  int       sock_;
};


// A UDP Echo Client socket. Sends a message to a host and gets it back, printing
// to stderr.
struct Client_socket: Socket
{
  using Socket::Socket;

  // Ctor. Create the socket and resolve the host address.
  Client_socket(std::string const& host_addr, int port)
    : Socket()
  {
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    inet_pton(AF_INET, host_addr.c_str(), &addr_.sin_addr);
    addr_.sin_port = htons(port);
  }

  // Puts the socket into a functional state; read from stdin and write to
  // the server address.
  void open()
  {
    int flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    // Connect to the server.
  }

  // Close the socket. Send a 'goodbye' message and close the file descriptor.
  void close()
  {
    ::close(sock_);
  }

  // Send a message to the server.
  bool send()
  {
    if (sendto(sock_, buf_, std::strlen(buf_) + 1, 0,
      (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
      perror("Client_socket send");
      return false;
    }
    else
      return true;
  }

  // Receive a message from the server.
  bool recv()
  {
    clear_buff();
    struct sockaddr_in recv_addr;
    std::memset(&recv_addr, 0, sizeof(recv_addr));
    if (recvfrom(sock_, buf_, MAX_PACKET_SIZE, 0,
      (struct sockaddr*)&recv_addr, &addr_len_) < 0) {
      perror("Client_socket recvfrom\n");
      return false;
    }
    else {
      std::cerr << buf_ << '\n';
      return true;
    }
  }
};
