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


int
fp_port_is_up(fp::Port* p)
{
  return p->is_up();
}


int
fp_port_is_down(fp::Port* p)
{
  return p->is_down();
}


} // extern "C"
