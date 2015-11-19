#include "buffer.hpp"

namespace fp
{


namespace Buffer
{

// Ctor for the base packet buffer.
Base::Base(unsigned char* data)
{
  data_ = data;
}


// Virtual Dtor for base packet buffer.
Base::~Base()
{
  if (data_)
    delete[] data_;
}

} // end namespace buffer

} // end namespace fp