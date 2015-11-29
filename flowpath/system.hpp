#ifndef FP_SYSTEM_HPP
#define FP_SYSTEM_HPP

#include "port.hpp"
#include "table.hpp"


extern "C" fp::Dataplane* fp_get_dataplane(std::string const&);
extern "C" fp::Port*      fp_get_port(std::string const&);
extern "C" void           fp_output_port(fp::Context*, std::string const&);
extern "C" fp::Table*     fp_create_table(fp::Dataplane*, int, int, fp::Table::Type);
extern "C" void           fp_delete_table(fp::Dataplane*, fp::Table*);
extern "C" void           fp_add_flow(fp::Table*, void*, void*);
extern "C" void           fp_remove_flow(fp::Table*, void*);
extern "C" void           fp_goto_table(fp::Context*, fp::Table*);
extern "C" void           fp_bind(fp::Context*, int, int, int, int);
extern "C" void           fp_bind_hdr(fp::Context*, int, int);
extern "C" void           fp_load(fp::Context*, int);
extern "C" void           fp_gather(fp::Context*, ...);
extern "C" void           fp_advance(fp::Context*, int);

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