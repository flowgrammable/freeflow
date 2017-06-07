#include "libsteve.hpp"
#include <exception>

// ---------------------------------------------------------------------------//
//            Packet modification

uint32_t
fp_get_packet_in_port(fp::Context* c)
{
  return c->in_port();
}

uint32_t
fp_get_packet_in_phys_port(fp::Context* c)
{
  return c->in_phy_port();
}

// Advances the current header offset by 'n' bytes.
void
fp_advance_header(fp::Context* cxt, std::uint16_t n)
{
  // std::cout << "ADV: " << n << std::endl;
  cxt->advance(n);
}


// Binds the current header offset to given identifier.
void
fp_bind_header(fp::Context* cxt, int id)
{
  cxt->bind_header(id);
}


// Binds a given field index to a section in the packet contexts raw
// packet data. Using the current cxt offset, relative field offset, and field
// length we can grab exactly what we need.
//
// Returns the pointer to the byte at that specific location
void
fp_bind_field(fp::Context* cxt, int id, std::uint16_t off, std::uint16_t len)
{
  // std::cout << "BINDING FIELD\n";
  // Get field requires an absolute offset which is the context's current offset
  // plus the relative offset passed to this function.
  int abs_off = cxt->offset() + off;

  if (abs_off > cxt->size())
  {
    throw std::exception();
  }

  // We bind fields using their absolute offset since this is the only way we
  // can recover the absolute offset when we need to look up the binding later.
  //
  // FIXME: There needs to be a way to store the relative offset instead of the
  // absolute offset.
  cxt->bind_field(id, abs_off, len);
}


// Bind twice: once with the original field and again with its aliased name.
void
fp_alias_bind(fp::Context* cxt, int original, int alias,
              std::uint16_t off, std::uint16_t len)
{
  // Get field requires an absolute offset which is the context's current offset
  // plus the relative offset passed to this function.
  int abs_off = cxt->offset() + off;
  // We bind fields using their absolute offset since this is the only way we
  // can recover the absolute offset when we need to look up the binding later.
  //
  // FIXME: There needs to be a way to store the relative offset instead of the
  // absolute offset.
  cxt->bind_field(original, abs_off, len);
  cxt->bind_field(alias, abs_off, len);
}



fp::Byte*
fp_read_field(fp::Context* cxt, int fld, fp::Byte* ret)
{
  // std::cout << "READING FIELD";
  // Lookup the field in the context.
  fp::Binding b = cxt->get_field_binding(fld);
  fp::Byte* p = cxt->get_field(b.offset);
  // Convert to native byte ordering.
  // Copy the value to a temporary.
  std::copy(p, p + b.length, ret);
  fp::network_to_native_order(ret, b.length);

  return ret;
}


void
fp_set_field(fp::Context* cxt, int fld, int len, fp::Byte* val)
{
  fp::Binding& b = cxt->get_field_binding(fld);

  // Copy the new data into the packet at the appropriate location.
  fp::Byte* p = cxt->get_field(b.offset);
  // Convert native to network order after copying.
  std::copy(val, val + len, p);
  fp::native_to_network_order(p, len);
  // Update the length if it changed (which it shouldn't).
  b.length = len;
}


// Clear the context's action list.
void
fp_clear(fp::Context* cxt)
{
  cxt->clear_actions();
}


// ---------------------------------------------------------------------------//
//            Writing actions

void
fp_write_drop(fp::Context* cxt)
{
  fp::Output_action out{0xfffffff0};
  fp::Action action;
  action.value.output = out;
  action.type = fp::Action::Type::OUTPUT;
  cxt->write_action(action);
}


// FIXME: Change that port action when there actually is a flood
// port.
void
fp_write_flood(fp::Context* cxt)
{
  fp::Output_action out{0xfffffff0};
  fp::Action action;
  action.value.output = out;
  action.type = fp::Action::Type::OUTPUT;
  cxt->write_action(action);
}


void
fp_write_output(fp::Context* cxt, unsigned int id)
{
  fp::Output_action out{id};
  fp::Action action;
  action.value.output = out;
  action.type = fp::Action::Type::OUTPUT;
  cxt->write_action(action);
}


// NOTE: We do not flip the bytes to the correct order here.
// Trust that that gets done during application.
void
fp_write_set_field(fp::Context* cxt, int fld, int len, fp::Byte* buf)
{
  fp::Binding& b = cxt->get_field_binding(fld);
  // Update the length if it changed (which it shouldn't).
  b.length = len;
  fp::Set_action set(fp::Packet_memory, b.offset, b.length, buf);

  cxt->write_action(fp::Action(set));
}
