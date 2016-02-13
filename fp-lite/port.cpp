#include "port.hpp"

// -------------------------------------------------------------------------- //
// Application interface

extern "C"
{

int
fp_port_get_id(fp::Port* p)
{
  return p->id();
}


} // extern "C"
