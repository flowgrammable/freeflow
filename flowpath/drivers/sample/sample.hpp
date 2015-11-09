#ifndef SAMPLE_HPP
#define SAMPLE_HPP

#include <iostream>
#include "lang/include.hpp"
using namespace fp;
using App_cxt = Application_context<2, 5, 4>;
struct eth
{
  int src;
  int dest;
  int protocol;
};
struct ipv4
{
  int src;
  int dest;
  int protocol;
};
void eth_d(App_cxt& _cxt_);
void ipv4_d(App_cxt& _cxt_);
extern "C++" Exact_table t1;
Exact_table t1 = Exact_table({0, 1, 4}, {{fp::make_key(1, 2, 1), fp::Flow()}, {fp::make_key(1, 2, 1), fp::Flow()}});
extern "C++" void eth_d(App_cxt& _cxt_)
{
  eth& _header_ = *reinterpret_cast<eth*>(_cxt_.get_current_byte());
  __bind_header(_cxt_, 0, 12);
  __bind_offset(_cxt_, 0, 0);
  __bind_offset(_cxt_, 1, 4);
  __bind_offset(_cxt_, 2, 8);
  switch (_header_.protocol)
  {
    case 0: 
    {
      __advance(_cxt_, 12);
      __decode(_cxt_, ipv4_d);
      break;
    }
    case 1: 
    {
      __advance(_cxt_, 12);
      __decode(_cxt_, ipv4_d);
      break;
    }
  }
}
extern "C++" void ipv4_d(App_cxt& _cxt_)
{
  ipv4& _header_ = *reinterpret_cast<ipv4*>(_cxt_.get_current_byte());
  __bind_header(_cxt_, 1, 12);
  __bind_offset(_cxt_, 3, 0);
  __bind_offset(_cxt_, 4, 4);
  switch (_header_.protocol)
  {
    case 0: 
    {
      __advance(_cxt_, 12);
      __match(_cxt_, t1);
      break;
    }
    case 1: 
    {
      __advance(_cxt_, 12);
      __decode(_cxt_, eth_d);
      break;
    }
  }
}
void egress(fp::Context* _cxt_){
   std::cout << _cxt_ << '\n';
   delete _cxt_;
}
Context* ingress(fp::Context* _cxt_){
   App_cxt* a = new App_cxt(*_cxt_);
   delete _cxt_;
   _cxt_ = a;
   __decode(*a, eth_d);
   egress(_cxt_);
}



#endif