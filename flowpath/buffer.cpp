#include "buffer.hpp"

#include <cstring>

namespace fp
{

namespace Buffer
{

// Ctor for the base packet buffer.
Base::Base() {
  bytes_ = 0;
  data_ = nullptr;
}

// Dtor for base packet buffer.
Base::~Base() {
}


// Ctor for simple 'allocated' packet buffer.
Simple::Simple(const uint8_t* data, int bytes) {
  data_ = new uint8_t[bytes];
  bytes_ = bytes;
  memcpy(data_, data, bytes);
}

// Dtor for simple packet buffer must release
//  before allowing Base to zero pointers.
Simple::~Simple() {
  delete data_;
}


Odp::Odp(uint8_t* buf, int bytes) {
  data_ = buf;
  bytes_ = bytes;
}

Odp::Odp(const uint8_t* buf, int bytes) {
  // TODO: provide a constant buffer interface to ODP!
  data_ = const_cast<uint8_t*>(buf);
  bytes_ = bytes;
}

Odp::~Odp() {
}


Pcap::Pcap(uint8_t* buf, int bytes) :
  Simple(buf, bytes), wire_bytes_(bytes) {
}

Pcap::~Pcap() {
}

} // end namespace buffer

} // end namespace fp
