#include "buffer.hpp"

#include <string.h>

namespace fp
{


namespace Buffer
{

// Ctor for the base packet buffer.
Base::Base(unsigned char* data)
{
  memset(data_, 0, 1024);
  memcpy(data_, data, sizeof(data));
}


// Virtual Dtor for base packet buffer.
Base::~Base()
{
  if (data_)
    delete[] data_;
}

} // end namespace buffer

} // end namespace fp