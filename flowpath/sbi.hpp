#ifndef FP_SBI_HPP
#define FP_SBI_HPP

#include "context.hpp"
#include "dataplane.hpp"
#include "table.hpp"
#include "types.hpp"

// This module defines the set of openflow standard
// controller-to-switch messages

namespace fp 
{

struct Features;
struct Device;

// ------------------------------------------------------------- //
//        Controller-to-switch ofp messages
// ------------------------------------------------------------- //

// Features* get_features(System*);

Context* get_context();

// ------------------------------------------------------------- //
//        Configuration

// Table configuration
enum Table_kind
{
  exact,
  prefix,
  wildcard
};


Table* request_table(Table_kind, int size);
void release_table(Table*);
Port* new_port(Device*);

// Adding/removing applications
void add_app(std::string);
void rmv_app(std::string);

// starting stopping dataplane
void dataplane_stop(Dataplane*);
void dataplane_start(Dataplane*);


// ------------------------------------------------------------- //
//         Modify-state


void add_flow(Table*, Flow*);
void modify_flow(Table*, Flow*);

// Port modification
Port* add_port(int); // port #
void close_port(int); // close a port #
void delete_port(Port*);
void up_port(Port*);
void down_port(Port*);

// set port stuff for udp
Port* add_udp_port(int);
void  close_udp_port(int);
void  delete_udp_port(Port*);
void  up_udp_port(Port*);
void  down_udp_port(Port*);


// ------------------------------------------------------------ //
//        Packet read/write functions

void write_msbf(int offset, std::uint16_t, int len, Byte const*);
void write_msbf(int offset, std::uint32_t, int len, Byte const*);
void write_msbf(int offset, std::uint64_t, int len);

void read_msbf(int offset, std::uint16_t&, int len, Byte const*);
void read_msbf(int offset, std::uint32_t&, int len, Byte const*);
void read_msbf(int offset, std::uint64_t&, int len, Byte const*);

void read_lsbf(int offset, std::uint16_t&, int len, Byte const*);
void read_lsbf(int offset, std::uint32_t&, int len, Byte const*);
void read_lsbf(int offset, std::uint64_t&, int len, Byte const*);

void write_lsbf(int offset, std::uint16_t, int len, Byte const*);
void write_lsbf(int offset, std::uint32_t, int len, Byte const*);
void write_lsbf(int offset, std::uint64_t, int len, Byte const*);

void drop_packet(Packet*);


} // end namespace fp


#endif