#include "buffer.hpp"

#include <string.h>

namespace fp
{


namespace Buffer
{

// Ctor for the base packet buffer.
Base::Base(unsigned char* data)
  : data_(data)
{

}


// Virtual Dtor for base packet buffer.
Base::~Base()
{

}

} // end namespace buffer

} // end namespace fp