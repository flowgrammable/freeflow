#ifndef FP_SYSTEM_HPP
#define FP_SYSTEM_HPP

#include "port.hpp"
#include "table.hpp"
#include "action.hpp"

extern "C"
{

// Apply actions.
void           fp_drop(fp::Context*);
void           fp_flood(fp::Context*);
void           fp_set_field(fp::Context*, int, void*);
void           fp_clear(fp::Context*);
void           fp_goto(fp::Context*, fp::Table*);
void           fp_output_port(fp::Context*, fp::Port*);

// Write actions.
void           fp_write_drop(fp::Context*);
void           fp_write_flood(fp::Context*);
void           fp_write_set_field(fp::Context*, int, void*);
void           fp_write_clear(fp::Context*);
void           fp_write_goto(fp::Context*, fp::Table*);
void           fp_write_output_port(fp::Context*, fp::Port*);

void           fp_apply(fp::Context*, fp::Action);
void           fp_write(fp::Context*, fp::Action);

// System queries.
fp::Dataplane* fp_get_dataplane(std::string const&);
fp::Port*      fp_get_port(char const*);
void           fp_gather(fp::Context*, int, ...);

// Flow tables.
fp::Table*     fp_create_table(fp::Dataplane*, int, int, fp::Table::Type);
void           fp_delete_table(fp::Dataplane*, fp::Table*);
void           fp_add_flow(fp::Table*, void*, void*);
void           fp_remove_flow(fp::Table*, void*);

// Header tracking.
void           fp_advance_header(fp::Context*, std::uint16_t);
void           fp_bind_header(fp::Context*, int);
fp::Byte*      fp_bind_field(fp::Context*, int, std::uint16_t, std::uint16_t);


} // extern "C"


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
