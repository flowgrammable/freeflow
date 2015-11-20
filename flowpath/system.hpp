#ifndef FP_SYSTEM_HPP
#define FP_SYSTEM_HPP

#include "port.hpp"
#include "table.hpp"


namespace fp 
{

struct Dataplane;

// Port management functions.
//
Port*             create_port(Port::Type, std::string const&);
void 	            delete_port(Port::Id);
extern "C" Port*  get_port(std::string const&);
extern "C" void   output_port(Context*, std::string const&);

// Data plane management functions.
//
Dataplane*             create_dataplane(std::string const&, std::string const&);
void			             delete_dataplane(std::string const&);
extern "C" Dataplane*  get_dataplane(std::string const&);

// Application management functions.
//
void load_application(std::string const&);
void unload_application(std::string const&);

// Table management functions.
//
extern "C" Table* create_table(Dataplane*, int, int, Table::Type);
extern "C" void   delete_table(Dataplane*, Table*);

// Flow management functions.
//
extern "C" void   add_flow(Table*, void*, void*);
extern "C" void   remove_flow(Table*, void*);
extern "C" void   goto_table(Context*, Table*);

// Context management functions.
//
extern "C" void   bind(Context*, int, int, int, int);
extern "C" void   bind_hdr(Context*, int, int);
extern "C" void   load(Context*, int);
extern "C" void   gather(Context*, ...);
extern "C" void   advance(Context*, int);

} // end namespace fp

#endif