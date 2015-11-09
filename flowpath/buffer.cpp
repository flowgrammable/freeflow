#include "buffer.hpp"

#include <cstring>

namespace fp
{

// Constructor for the base packet buffer.
//
// TODO: Figure out a way to not have to copy this.
Buffer::Base::Base(unsigned char* data)
{
	std::memcpy(data_, data, sizeof(data));
}

} // end namespace fp