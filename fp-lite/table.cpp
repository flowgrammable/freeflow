#include "table.hpp"

namespace fp
{

// FIXME: Key's can't be user defined types.
//
// // Initialize the first len bytes of the key with those
// in the given buffer, and zero-fill the remainder.
// Key::Key(Byte const* buf, int len)
// {
//   std::fill(std::copy(buf, buf + len, data), data + key_size, 0);
// }


// Returns a reference to a flow. If no flow matches the
// key, the table-miss flow is returned.
Flow&
Hash_table::search(Key const& k)
{
  auto iter = std::unordered_map<Key, Flow, Key_hash>::find(k);
  if (iter == end())
    return miss_;
  else
    return iter->second;
}


// Returns a reference to a flow. If no flow matches the
// key, the table-miss flow is returned.
Flow const&
Hash_table::search(Key const& k) const
{
  auto iter = std::unordered_map<Key, Flow, Key_hash>::find(k);
  if (iter == end())
    return miss_;
  else
    return iter->second;
}


// If an equivalent flow entry exists, no action is taken.
// Returns a reference to the inserted (or previous) flow.
void
Hash_table::insert(Key const& k, Flow const& f)
{
  Map::insert({k, f});
}


// If no such entry exists, no action is taken.
void
Hash_table::erase(Key const& k)
{
  Map::erase(k);
}

} // namespace fp
