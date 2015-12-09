
#ifndef FREEFLOW_JSON_HPP
#define FREEFLOW_JSON_HPP

// The Freeflow implementation of JSON is designd for optimized
// parsing and memory representation. Each object is exactly
// the size of 1 int and 2 pointers (12 bytes on a 32-bit system,
// and 20 bits on a 64 bit system).
//
// The parser requires a full character buffer and allocates nodes 
// as views into that buffer.
//
// This does not implement the full JSON specification. It does
// not handle international characters, and does not distinguish
// between literal values (true, false, null, numbers or strings).
//
// The object implementation is not an unordered set of key/value
// pairs, and does not provide efficient lookup.

#include "freeflow/memory.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <iosfwd>
#include <string>
#include <vector>
#include <unordered_map>

namespace ff
{

namespace json
{

// -------------------------------------------------------------------------- //
//                            Value representation

// The kinds of JSON vlaues.
enum Kind 
{ 
  identifier_value,
  number_value, 
  string_value,
  pair_value, 
  array_value, 
  object_value 
};


// Every value stores its associated kind.
struct Value
{
  Value(Kind k)
    : kind(k)
  { }

  Kind kind;
};


// A terminal in the JSON grammar. This includes identifiers,
// numbers and strings. 
struct Terminal : Value
{
  Terminal(Kind k, char const* f, char const* l)
    : Value(k), first(f), last(l)
  { }

  int size() const { return last - first; }

  char const* first;
  char const* last;
};


// Returns true when `a` and `b` have the same sequence of 
// characters.
inline bool 
operator==(Terminal const& a, Terminal const& b)
{
  return !std::strncmp(a.first, b.first, std::min(a.size(), b.size()));
}


inline bool 
operator!=(Terminal const& a, Terminal const& b)
{
  return std::strncmp(a.first, b.first, std::min(a.size(), b.size()));
}


// Returns true if the identifier has the same sequence of 
// characters as `str`.
inline bool 
operator==(Terminal const& t, char const* str)
{
  return !std::strncmp(t.first, str, t.size());
}


inline bool 
operator==(char const* str, Terminal const& t)
{
  return !std::strncmp(t.first, str, t.size());
}


inline bool 
operator!=(Terminal const& t, char const* str)
{
  return std::strncmp(t.first, str, t.size());
}


inline bool 
operator!=(char const* str, Terminal const& t)
{
  return std::strncmp(t.first, str, t.size());
}


// An uninterpreted identifier. This includes the usual JSON values 
// null, true, and false. We also allow abitrary identifiers as 
// sequences of characters.
struct Identifier : Terminal
{
  static constexpr Kind value_kind = identifier_value;

  Identifier(char const* f, char const* l)
    : Terminal(value_kind, f, l)
  { }
};


// An uninterpreted number. Represented by a sequence of characters.
struct Number : Terminal
{
  static constexpr Kind value_kind = number_value;

  Number(char const* f, char const* l)
    : Terminal(value_kind, f, l)
  { }
};


// A string, represented by a sequence of characters. The enclosing
// quotes are not part of the value.
struct String : Terminal
{
  static constexpr Kind value_kind = string_value;

  String(char const* f, char const* l)
    : Terminal(value_kind, f, l)
  { }

  std::size_t hash() const;
  std::string str() const { return std::string(first, last); }

  // Iterators
  char const* begin() const { return first; }
  char const* end() const { return last; }
};


struct String_hash
{
  std::size_t operator()(String const* s) const
  {
    return s->hash();
  }
};


struct String_eq
{
  bool operator()(String const* a, String const* b) const
  {
    return *a == *b;
  }
};


// A key value pair in an object points to the first value
// (key), and the second value. Note that we allow any value
// to appear as the key since we aren't hashin on it.
struct Pair : Value
{
  static constexpr Kind value_kind = pair_value;

  Pair(String* k, Value* v)
    : Value(value_kind), key(k), value(v)
  { }

  String* key;
  Value*  value;
};


// An array is a null-terminated sequence of nodes. Note that
// the next pointer always points to an array value.
//
// An empty array is representedby an array whose both
// pointers are null.
//
// This data structure does not provide efficient indexing. Although
// operator[] is provided, be aware that it is a linear time operation.
struct Array : Value
{
  static constexpr Kind value_kind = array_value;

  Array(Value* v, Array* n)
    : Value(value_kind), value(v), next(n)
  { }

  bool empty() const { return !value; }
  int size() const;

  Value* operator[](int) const;

  Value* value;
  Array* next;
};


// An object map provides efficient lookup for objects. Note
// that this is not part of an object, but a lookup table that
// can be constructed from a parsed Object.
struct Map : std::unordered_map<String*, Value*, String_hash, String_eq>
{
  using Base = std::unordered_map<String*, Value*, String_hash, String_eq>;
  
  using iterator       = typename Base::iterator;
  using const_iterator = typename Base::const_iterator;

  using Base::unordered_map;

  Value* operator[](char const*) const;
  Value* operator[](std::string const&) const;

  const_iterator find(char const*) const;
  const_iterator find(std::string const&) const;
};


inline Value*
Map::operator[](char const* k) const
{
  auto iter = find(k);
  return iter != end() ? iter->second : nullptr;
}


inline Value*
Map::operator[](std::string const& k) const
{
  auto iter = find(k);
  return iter != end() ? iter->second : nullptr;
}


inline auto 
Map::find(char const* k) const -> const_iterator
{
  String s(k, k + std::strlen(k));
  return Base::find(&s);
}


inline auto 
Map::find(std::string const& k) const -> const_iterator
{
  String s(k.c_str(), k.c_str() + k.size());
  return Base::find(&s);
}


// An object is a null-terminated sequence of nodes. Note that the
// next pointer always points to an object value.
//
// An empty object is represented by an object whose both pointers
// are null.
//
// This data structure does not provide efficient lookup. Although
// operator[] is provided for key lookup, be aware that it is a linear
// time search.
struct Object : Value
{
  static constexpr Kind value_kind = object_value;

  Object(Pair* v, Object* n)
    : Value(value_kind), value(v), next(n)
  { }

  bool empty() const { return !value; }
  int size() const;
  Map map() const;

  Value* operator[](char const*) const;
  Value* operator[](std::string const&) const;

  Pair*   value;
  Object* next;
};


// -------------------------------------------------------------------------- //
//                                  Casting

// Convert v into a value of type T iff v is a value of that
// type. Otherwise, return nullptr.
template<typename T>
inline T*
as(json::Value* v)
{
  if (v->kind == T::value_kind)
    return static_cast<T*>(v);
  else
    return nullptr;
}


template<typename T>
inline T const*
as(json::Value const* v)
{
  if (v->kind == T::value_kind)
    return static_cast<T const*>(v);
  else
    return nullptr;
}


// Convert v into a value of type of T iff v is a value of
// that type. Otherwise, behavior is undefined.
template<typename T>
inline T*
cast(json::Value* v)
{
  assert(v->kind == T::value_kind);
  return static_cast<T*>(v);
}


template<typename T>
inline T const*
cast(json::Value const* v)
{
  assert(v->kind == T::value_kind);
  return static_cast<T const*>(v);
}


// -------------------------------------------------------------------------- //
//                               Streaming


std::ostream& operator<<(std::ostream&, Value const& v);


// -------------------------------------------------------------------------- //
//                          Document representation


// A JSON document maintains the allocation context for these nodes.
//
// TODO: Define a default constructor. This means selecting a default
// allocator that will be able to deallocate nodes when the document
// goes out of scope.
class Document
{
  // This union is used to compute the general ojbect size for
  // the JSON values. It is used only to slab allocation within
  // the document class.
  //
  // TODO: Consider aligning this so that an even number fit in the
  // cache line. Alternatively, since everything is a pair of pointers,
  // consider using intptrs to save the kind (smaller, but slower).
  union Rep
  {
    Terminal t; // Identifier, Number, and String
    Pair     p;
    Array    a;
    Object   o;
  };

public:
  Document(Allocator& a)
    : alloc_(&a)
  { }

  Value* parse(char const*&, char const*);
  Value* parse(char const*&);
  Value* parse(std::string&);

private:
  Identifier* make_identifier(char const*, char const*);
  Number* make_number(char const*, char const*);
  String* make_string(char const*, char const*);
  Pair* make_pair(String*, Value*);
  Array* make_array(Value*);
  Array* make_array(Value*, Array*);
  Object* make_object(Pair*);
  Object* make_object(Pair*, Object*);

  Value* parse_value(char const*&, char const*);
  Identifier* parse_identifier(char const*&, char const*);
  Number* parse_number(char const*&, char const*);
  String* parse_string(char const*&, char const*);
  String* parse_key(char const*&, char const*);
  Pair* parse_pair(char const*&, char const*);
  Array* parse_array(char const*&, char const*);
  Object* parse_object(char const*&, char const*);

private:
  void* allocate();

  Allocator* alloc_;
};

inline void*
Document::allocate()
{
  return alloc_->allocate(sizeof(Rep));
}


inline Identifier* 
Document::make_identifier(char const* first, char const* last)
{
  return new (allocate()) Identifier(first, last);
}


inline Number* 
Document::make_number(char const* first, char const* last)
{
  return new (allocate()) Number(first, last);
}


inline String* 
Document::make_string(char const* first, char const* last)
{
  return new (allocate()) String(first, last);
}


inline Pair* 
Document::make_pair(String* k, Value* v)
{
  return new (allocate()) Pair(k, v);
}


inline Array* 
Document::make_array(Value* v)
{
  return new (allocate()) Array(v, nullptr);
}


inline Array* 
Document::make_array(Value* v, Array* prev)
{
  Array* arr = make_array(v);
  if (prev)
    prev->next = arr;
  return arr;
}


inline Object* 
Document::make_object(Pair* p)
{
  return new (allocate()) Object(p, nullptr);
}


inline Object* 
Document::make_object(Pair* p, Object* prev)
{
  Object* obj = make_object(p);
  if (prev)
    prev->next = obj;
  return obj;
}


inline Value*
Document::parse(char const*& first, char const* last)
{
  return parse_value(first, last);
}


inline Value*
Document::parse(char const*& str)
{
  return parse(str, str + std::strlen(str));
}


inline Value* 
Document::parse(std::string& str)
{
  char const* first = str.c_str();
  Value* v = parse(first, first + str.size());
  str = std::string(first);
  return v;
}


// -------------------------------------------------------------------------- //
//                         Simplified construction

std::string make_object(std::initializer_list<std::pair<char const*, char const*>>);
std::string make_array(std::initializer_list<char const*>);


} // namespace json

} // namespace ffs

#endif
