#include "buffer.hpp"

#include <string.h>

namespace fp
{


namespace Buffer
{

// Ctor for the base packet buffer.
Base::Base(unsigned char* data, size_t len)
{
  data_ = new uint8_t[len];
  memcpy(data_, data, len);
}


// Virtual Dtor for base packet buffer.
Base::~Base()
{
  delete[] data_;
}

} // end namespace buffer

} // end namespace fp