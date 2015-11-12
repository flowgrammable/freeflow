#include "buffer.hpp"

#include <cstring>

namespace fp
{


namespace Buffer
{

// Ctor for the base packet buffer.
Base::Base(unsigned char* data)
{
  std::memset(data_, 0, FP_INIT_BUF);
  std::memcpy(data_, data, sizeof(data));
}


// Virtual Dtor for base packet buffer.
Base::~Base()
{
  if (data_)
    delete[] data_;
}

} // end namespace buffer

} // end namespace fp