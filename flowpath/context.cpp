#include "context.hpp"

namespace fp
{

Context::Context(Packet* p, Port::Id in, Port::Id in_phys, int tunn_id)
	: packet_(p), in_port(in), in_phy_port(in_phys), tunnel_id(tunn_id)
{ }

void
Context::write_metadata(uint64_t meta)
{
  metadata_.data = meta;
}


Metadata const&
Context::read_metadata()
{
  return this->metadata_;
}

} // namespace fp
