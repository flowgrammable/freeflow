#ifndef FP_PORT_ODP_HPP
#define FP_PORT_ODP_HPP

#include "port.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <new>

extern "C" {
#include <pcap.h>
//#include <assert.h>
}

class Context;

namespace entangle {

FILE *gzip_open(const char *path, const char *mode);

template <char const* str>
class Msg {
   public:
      static constexpr char const* HEAD = str ;

   public:
      Msg() : msg_() {}
      Msg( const std::string& m ) : msg_(m) {}

      Msg( const Msg& ) = default ;
      Msg( Msg&& ) = default ;
      Msg& operator=( const Msg& ) = default ;
      Msg& operator=( Msg&& ) = default ;

      const std::string& msg() const { return msg_ ; }

      friend std::ostream& operator<<( std::ostream& out, const Msg& m ) {
         out << Msg::HEAD << ": " << m.msg_ ;
         return out ;
      }

   private:
      std::string msg_ ;
};

constexpr char str_FileNotFound[] = "File Not Found";
constexpr char str_FileEmpty[] = "File Empty";
constexpr char str_BadDeref[] = "Bad Dereference";
constexpr char str_IncoTiming[] = "Inconsistent Timing File";

constexpr char const* link_names[] = { "None", "Unknown",
                                       "Ethernet", "Wifi", "PPP",
                                       "IPv4", "IPv6", "Raw" };

using FileNotFound = Msg<str_FileNotFound>;
using FileEmpty = Msg<str_FileEmpty>;
using BadDeref = Msg<str_BadDeref>;
using IncoTiming = Msg<str_IncoTiming>;

class PcapFile {
   public:
//      using PcapFileIterator = InputIterator<PcapFile>;
      using meta_type = pcap_pkthdr;
      using buf_type = uint8_t;
//      using value_type = uint8_t*;
//      using distance_type = int;
      enum Datalink { None, Unknown, Ethernet, Wifi, PPP, IPv4, IPv6, Raw };

   public:
      PcapFile(const char* capfile, const char* timefile = "") :
                  capfile_(capfile), timefile_(timefile),
                  cap_handle_( pcap_fopen_offline(gzip_open(capfile, "r"), errbuf) ),
                  time_handle_(timefile_),
                  pcap_header_(nullptr), pcap_buffer_(nullptr),
                  reads_(0), eof_(false)
      {
         if (cap_handle_ == nullptr) throw FileNotFound(errbuf);
      }

      // Disable copy construction and copy assignment:
      PcapFile() = delete;
      PcapFile(const PcapFile&) = delete;
      PcapFile& operator=(const PcapFile&) = delete;

      // Default move constructiona and move assignment:
      PcapFile(PcapFile&&) = default;
      PcapFile& operator=(PcapFile&&) = default;

      ~PcapFile() {
         pcap_close(cap_handle_);
         time_handle_.close();
      }

      Datalink datalink() const {
         if ( cap_handle_ == nullptr ) return None;
         switch ( pcap_datalink(cap_handle_) ) {
            case DLT_EN10MB: return Ethernet ;
            case DLT_IEEE802_11: return Wifi ;
            case DLT_PPP: return PPP ;
            case DLT_IPV4: return IPv4 ;
            case DLT_IPV6: return IPv6 ;
            case DLT_RAW: return Raw ;
            default: return Unknown ;
         }
      }

      bool empty() const { return eof_; }
      std::string filename() const { return capfile_; }
      size_t reads() const { return reads_; }

      int next() {
        if (eof_) return -2;
//        if (eof_) throw FileEmpty();

        int status = pcap_next_ex(cap_handle_, &pcap_header_, &pcap_buffer_);
        if (status != 1) {
          if (status == -2) {
            // end of file reached
            eof_ = true;
            return status;
          }
          else if (status == -1) {
            // error reading
            // pcap_geterr() or pcap_perror() may be called
            // with p as an argument to fetch or display the error text.
            throw BadDeref();
          }
          // error unknown
        }

        // succesful read
        ++reads_ ;
        update_ts();
        return status;
      }

      const meta_type *const peek_meta() const {return pcap_header_;}
      const buf_type *const peek_buf() const {return pcap_buffer_;}
      timespec get_ts() const {return ts_;}

   private:
      void update_ts() {
        // convert timeval (us) to timespec (ns)
        ts_.tv_sec = pcap_header_->ts.tv_sec;
        ts_.tv_nsec = pcap_header_->ts.tv_usec*1000;

        // import ns precision if available
        if (!time_handle_.is_open() || !time_handle_.good())
          return;

         std::string line;
         getline(time_handle_, line);
         if (!time_handle_.good()) {
           std::cerr << time_handle_.eof() << std::endl;
           std::cerr << time_handle_.fail() << std::endl;
           std::cerr << time_handle_.bad() << std::endl;
           return;
         }

         std::stringstream ss(line);
         int64_t sec;
         int64_t subsec;
         ss >> sec;
         ss.ignore(1);  // "."
         int precision = ss.tellg();
         ss >> subsec;
         precision = int(ss.tellg()) - precision;

         if (ts_.tv_sec != sec) {
           std::cerr << ts_.tv_sec << " != " << sec << std::endl;
           //throw IncoTiming();
           return;
         }
         if (ts_.tv_nsec != subsec) {
           std::cerr << ts_.tv_nsec << " != " << subsec << std::endl;
           //throw IncoTiming();
           return;
         }
         ts_.tv_nsec = subsec;
      }

      // filenames
      std::string capfile_;
      std::string timefile_;

      // handles
      pcap_t* cap_handle_;
      std::ifstream time_handle_;

      // current headers
      meta_type* pcap_header_;
      const buf_type* pcap_buffer_;
      timespec ts_;

      // status
      size_t reads_;
      bool eof_;
      char errbuf[PCAP_ERRBUF_SIZE];
};
} // namespace entangle


namespace fp
{

class Port_pcap : public Port
{
public:
  using Port::Port;

  // Constructors/Destructor.
  Port_pcap(Port::Id, std::string const&);
  ~Port_pcap() = default;

  // Disable copy construction and copy assignment:
  Port_pcap(const Port_pcap&) = delete;
  Port_pcap& operator=(const Port_pcap&) = delete;

  // Default move constructiona and move assignment:
  Port_pcap(Port_pcap&&) = default;
  Port_pcap& operator=(Port_pcap&&) = default;

  // Packet related funtions.
  int recv(Context*);
  int send(Context*);
  int rebound(Context*); // helper to return buffer

  // Port state functions.
  int open();
  int close();

  // Non-standard info query functions:
  std::string name() const { return handle_.filename(); }

private:
  // Data members.
//  odp_pktio_t pktio_; // ODP pktio device id (handle)
//  odp_pool_t pktpool_; // ODP packet pool device id (handle)
  std::string dev_name_; // device name of port (pktio)
  //odp_packet_t pkt_tbl_[MAX_PKT_BURST];

  entangle::PcapFile handle_;

  // Socket addresses.
  //struct sockaddr_in src_addr_;
  //struct sockaddr_in dst_addr_;

  // IO Buffers.
  //struct mmsghdr  messages_[UDP_BUF_SIZE];
  //struct iovec    iovecs_[UDP_BUF_SIZE];
  //char            buffer_[UDP_BUF_SIZE][UDP_MSG_SIZE];
  //struct timespec timeout_;
};

} // end namespace fp

#endif
