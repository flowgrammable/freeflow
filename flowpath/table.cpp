#include "table.hpp"

namespace fp
{

// Initialize the first len bytes of the key with those
// in the given buffer, and zero-fill the remainder.
Key::Key(Byte const* buf, int len)
{
  std::fill(std::copy(buf, buf + len, data), data + key_size, 0);
}


// Returns a reference to a flow. If no flow matches the
// key, the table-miss flow is returned.
inline Flow&
Hash_table::find(Key const& k)
{
  auto iter = std::unordered_map<Key, Flow, Key_hash>::find(k);
  if (iter == end())
    return miss_;
  else
    return iter->second;
}


// Returns a reference to a flow. If no flow matches the
// key, the table-miss flow is returned.
inline Flow const&
Hash_table::find(Key const& k) const
{
  auto iter = std::unordered_map<Key, Flow, Key_hash>::find(k);
  if (iter == end())
    return miss_;
  else
    return iter->second;
}


// If an equivalent flow entry exists, no action is taken.
// Returns a reference to the inserted (or previous) flow.
inline void
Hash_table::insert(Key const& k, Flow const& f)
{
  std::unordered_map<Key, Flow, Key_hash>::insert({k, f});
}


// If no such entry exists, no action is taken.
inline void
Hash_table::erase(Key const& k)
{
  erase(k);
}

} // namespace fp
