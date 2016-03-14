
#ifndef FP_TABLE_HPP
#define FP_TABLE_HPP

#include "flow.hpp"

#include <boost/functional/hash.hpp>

#include <cstring>
#include <algorithm>
#include <unordered_map>

// delete this later
#include <iostream>
#include <vector>



namespace fp
{

struct Flow;

// Determines the maximum size of the key.
constexpr size_t key_size = 128;


// A key is a sequence of bytes.
struct Key
{
  Key(Byte const*, int);

  Byte data[key_size];
};


// Returns true when two keys are equal.
inline bool
operator==(Key const& a, Key const& b)
{
  return !std::memcmp(a.data, b.data, key_size);
}


inline bool
operator!=(Key const& a, Key const& b)
{
  return std::memcmp(a.data, b.data, key_size);
}


// Computes the hash value of a key.
struct Key_hash
{
  // TODO: Cast the buffer as integers and unroll the loop.
  // Alternatively use SSE intrinsics to parallelize the
  // hash function.
  std::size_t operator()(Key const& k) const
  {
    std::size_t seed = 0;
    for (Byte const* p = std::begin(k.data); p != std::end(k.data); ++p)
      boost::hash_combine(seed, *p);
    return seed;
  }
};


// The abstract table interface.
struct Table
{
  enum Type { EXACT, PREFIX, WILDCARD };

  Table(Type t, int id, int k)
    : type_(t), id_(id), key_size_(k), miss_()
  { }

  virtual ~Table() { }

  virtual Flow& search(Key const&) = 0;
  virtual Flow const& search(Key const&) const = 0;
  virtual void insert(Key const&, Flow const&) = 0;
  virtual void erase(Key const&) = 0;
  void insert_miss(Flow const& f) { miss_ = f; }
  void erase_miss() { miss_ = Flow(); }

  Type type()     const { return type_; }
  int  key_size() const { return key_size_; }
  Flow miss()     const { return miss_; }
  int  id()       const { return id_; }

  Type type_;
  int id_;
  int key_size_;
  // NOTE: The default constructed Flow contains the miss rule as its instruction.
  Flow miss_;   // The miss rule
};


// An exact match table.
//
// TODO: Use an open-address table.
//
// TODO: Support equivalent flows with multiple priorities.
//
// TODO: Support move semantics for flows.
//
// TODO: All of our tables match the same headers. OpenFlow
// requires those matches to be translated into OXM's but
// we want to be protocol agnostic. How do we solve this
// problem?
struct Hash_table : Table, private std::unordered_map<Key, Flow, Key_hash>
{
  using Map = std::unordered_map<Key, Flow, Key_hash>;

  Hash_table(int id, int size, int k)
    : Table(Table::EXACT, id, k), Map(size)
  { }

  Flow&       search(Key const&);
  Flow const& search(Key const&) const;

  void insert(Key const&, Flow const&);
  void erase(Key const&);
};



} // end namespace fp





#endif
