#ifndef FP_DATAPLANE_HPP
#define FP_DATAPLANE_HPP

#include <list>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#include "port.hpp"
#include "application.hpp"
#include "table.hpp"

namespace fp
{


struct Dataplane
{
  // Data plane name.
  std::string name_; 

  std::map<std::string, Table*> tables;

  // Application.
  Application* app_;

  Dataplane(std::string const&, std::string const&);
  ~Dataplane();

  void add_port(Port*);
  void remove_port(Port*);

  void up();
  void down();

  void configure();

  Application* app();
  std::string name() const;
};

using Dataplane_table = std::unordered_map<std::string, Dataplane*>;

extern "C" Table* create_table(Dataplane*, int, int, Table::Type);
extern "C" void   add_flow(Table*, void*, void*);
extern "C" void   del_flow(Table*, void*);
extern "C" Port*  get_port(std::string const&);
extern "C" void   output_port(Context*, std::string const&);
extern "C" void   goto_table(Context*, Table*);
extern "C" void   bind(Context*, int, int, int, int);
extern "C" void   bind_hdr(Context*, int, int);
extern "C" void   load(Context*, int);
extern "C" void   gather(Context*, ...);
extern "C" void   advance(Context*, int);

} // namespace

#endif
