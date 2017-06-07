#ifndef FP_LIBSTEVE_HPP
#define FP_LIBSTEVE_HPP

#include "action.hpp"
#include "types.hpp"
#include "context.hpp"
#include "endian.hpp"

extern "C"
{

uint32_t       fp_get_packet_in_port(fp::Context*);
uint32_t       fp_get_packet_in_phys_port(fp::Context*);

void           fp_advance_header(fp::Context*, std::uint16_t);
void           fp_bind_header(fp::Context*, int);
void           fp_bind_field(fp::Context*, int, std::uint16_t, std::uint16_t);
void           fp_alias_bind(fp::Context*, int, int, std::uint16_t, std::uint16_t);
fp::Byte*      fp_read_field(fp::Context*, int, fp::Byte*);
void           fp_set_field(fp::Context*, int, int, fp::Byte*);
void           fp_clear(fp::Context*);

// Write actions.
void           fp_write_drop(fp::Context*);
void           fp_write_flood(fp::Context*);
void           fp_write_set_field(fp::Context*, int, int, fp::Byte*);
void           fp_write_output(fp::Context*, unsigned int);

}

#endif
