#ifndef FP_SYSTEM_HPP
#define FP_SYSTEM_HPP

#include "port.hpp"
#include "table.hpp"


extern "C" Dataplane* fp_get_dataplane(std::string const&);
extern "C" Port*      fp_get_port(std::string const&);
extern "C" void       fp_output_port(Context*, std::string const&);
extern "C" Table*     fp_create_table(Dataplane*, int, int, Table::Type);
extern "C" void       fp_delete_table(Dataplane*, Table*);
extern "C" void       fp_add_flow(Table*, void*, void*);
extern "C" void       fp_remove_flow(Table*, void*);
extern "C" void       fp_goto_table(Context*, Table*);
extern "C" void       fp_bind(Context*, int, int, int, int);
extern "C" void       fp_bind_hdr(Context*, int, int);
extern "C" void       fp_load(Context*, int);
extern "C" void       fp_gather(Context*, ...);
extern "C" void       fp_advance(Context*, int);

namespace fp 
{

struct Dataplane;

// Port management functions.
//
Port*             create_port(Port::Type, std::string const&);
void 	            delete_port(Port::Id);

// Data plane management functions.
//
Dataplane*             create_dataplane(std::string const&, std::string const&);
void			             delete_dataplane(std::string const&);

// Application management functions.
//
void load_application(std::string const&);
void unload_application(std::string const&);

} // end namespace fp

#endif