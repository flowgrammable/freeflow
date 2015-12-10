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

const int MAX_PACKET_SIZE = 1522;
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
  Socket() { }
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

// A UDP Echo Server socket.
struct Server_socket : Socket
{
  using Socket::Socket;
  using Client_list = std::vector<Socket::Address>;

  // Ctor. Establish port and create address.
  Server_socket(int port)
    : port_(port)
  {
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    int optval = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval,
      sizeof(int));
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htons(INADDR_ANY);
    addr_.sin_port = htons(port);
  }

  // Puts the socket into a functional state; listening.
  void open()
  {
    if (::bind(sock_, (struct sockaddr*) &addr_, sizeof(addr_)) < 0)
      perror("ERR: Binding server socket\n");
    else {
      int flags = fcntl(sock_, F_GETFL, 0);
      fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
      listen(sock_, 5);
    }
  }

  // Closes the sockets file descriptor, stopping I/O.
  void close()
  {
    ::close(sock_);
  }

  // Attempts to read a new message. Since this has been configured to be a non-
  // blocking socket, it will not wait if the recvfrom call fails (for expected
  // reasons; EAGAIN || EWOULDBLOCK).
  bool recv()
  {
    // Clear the buffer and client addr struct.
    clear_buff();
    std::memset(&client_, 0, sizeof(client_));
    // Attempt to receive.
    if (recvfrom(sock_, buf_, MAX_PACKET_SIZE, 0,
      (struct sockaddr*)&client_, &addr_len_) < 0) {
      // If we end up here, there is no message or an error occurred.
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        throw std::string("ERR: Recvfrom\n");
      else
        return false;
    }
    else {
      // We have a new message.
      if (std::memcmp(buf_, &HELLO, sizeof(HELLO)) == 0 ||
        std::memcmp(buf_, &GOODBYE, sizeof(GOODBYE)) == 0)
        return false;
      else
        return true;
    }
  }

  // Sends the previously received message. This assumes that the call to 'recv'
  // succeeded, that is, returned 'true'. Calling send after a failed 'recv'
  // gives way to undefined behavior.
  bool send()
  {
    if (sendto(sock_, buf_, strlen(buf_) + 1, 0,
      (struct sockaddr*)&client_, addr_len_) < 0) {
      std::cerr << "ERR: Server sendto\n";
      return false;
    }
    return true;
  }

  // The port.
  int             port_;
  // Client address.
  Socket::Address client_;
};


// A UDP Echo Client socket.
struct Client_socket: Socket
{
  using Socket::Socket;

  // Ctor. Create the socket and resolve the host address.
  Client_socket(std::string const& host_addr, int port)
  {
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    std::memset(&addr_, 0, sizeof(addr_));
    struct hostent* host = gethostbyname(host_addr.c_str());
    addr_.sin_family = AF_INET;
    std::memcpy(&addr_.sin_addr.s_addr, host->h_addr, host->h_length);
    addr_.sin_port = htons(port);
  }

  // Puts the socket into a functional state; read from stdin and write to
  // the server address.
  void open()
  {
    // Connect to the server.
    if (connect(sock_, (const struct sockaddr*)&addr_, addr_len_) < 0)
        perror("ERR: Client connect\n");
    #if 0
    else {
      // Send a 'hello' message.
      clear_buff();
      std::memset(buf_, HELLO, sizeof(HELLO));
      if (sendto(sock_, buf_, strlen(buf_) + 1, 0,
        (struct sockaddr*)&addr_, sizeof(addr_)) < 0)
        std::cerr << "ERR: Client sendto(HELLO)\n";
      else {
        clear_buff();
        if (recvfrom(sock_, buf_, MAX_PACKET_SIZE, 0,
          (struct sockaddr*)&addr_, &addr_len_) < 0) {
          if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("ERR: Client recvfrom\n");
        }
        else
          std::cerr << "Connected to server\n";
      }
    }
    #endif
  }

  // Close the socket. Send a 'goodbye' message and close the file descriptor.
  void close()
  {
    #if 0
    clear_buff();
    std::memset(buf_, GOODBYE, sizeof(GOODBYE));
    if (sendto(sock_, buf_, strlen(buf_) + 1, 0,
      (struct sockaddr*)&addr_, sizeof(addr_)) < 0)
      std::cerr << "ERR: Client sendto(GOODBYE)\n";
    #endif
    ::close(sock_);
  }

  // Send a message to the server.
  bool send()
  {
    if (sendto(sock_, buf_, std::strlen(buf_) + 1, 0,
      (struct sockaddr*)&addr_, sizeof(addr_)) < 0)
      std::cerr << "ERR: Client send\n";
    return true;
  }

  // Receive a message from the server.
  bool recv()
  {
    clear_buff();
    if (recvfrom(sock_, buf_, MAX_PACKET_SIZE, 0,
      (struct sockaddr*)&addr_, &addr_len_) < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        perror("ERR: Client recvfrom\n");
      return false;
    }
    #if 0
    else
      std::cerr << buf_ << '\n';
    #endif

    return true;
  }
};
