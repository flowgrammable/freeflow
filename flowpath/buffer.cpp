#include "buffer.hpp"

#include <string.h>

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
  bytes_ = 0;
  data_ = nullptr;
}


// Ctor for simple 'allocated' packet buffer.
Simple::Simple(uint8_t* data, int bytes) : Base() {
  data_ = new uint8_t[bytes];
  memcpy(data_, data, bytes);
}

// Dtor for simple packet buffer must release
//  before allowing Base to zero pointers.
Simple::~Simple() {
  delete data_;
}


Odp::Odp(uint8_t* buf, int bytes) : Base() {
  data_ = buf;
  bytes_ = bytes;
}

Odp::~Odp() {

}

} // end namespace buffer

} // end namespace fp
