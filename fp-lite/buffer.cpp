#include "buffer.hpp"


namespace fp
{

namespace Buffer_pool
{

Pool& 
get_pool(Dataplane* dp)
{
  static Pool p(1024 * 256 + 1024, dp);
  return p;
}


} // namespace Buffer_pol

} // namespace fp
