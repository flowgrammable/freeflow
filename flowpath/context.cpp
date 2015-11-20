#include "context.hpp"

namespace fp
{

Context::Context(Packet* p, Port::Id in, Port::Id in_phys, int tunn_id,
								 int max_headers, int max_fields)
	: packet_(p), in_port(in), in_phy_port(in_phys), tunnel_id(tunn_id),
		hdr_(max_headers), fld_(max_fields)
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
