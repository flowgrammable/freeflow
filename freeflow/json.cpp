
#include "json.hpp"
#include "format.hpp"

#include <cassert>
#include <cctype>
#include <iostream>

#include <boost/functional/hash.hpp>

namespace ff
{

namespace json
{

namespace
{

// Returns the number of elements in the container p.
template<typename T>
inline int 
count(T const* p)
{
  if (p->empty())
    return 0;
  int n = 0;
  while (p) {
    p = p->next;
    ++n;
  }
  return n;

}


// Returns the value at the nth index in a, or nullptr if n is
// not within a.
Value*
at(Array const* a, int n)
{
  if (n < 0)
    return nullptr;
  if (a->empty())
    return nullptr;

  while (n != 0 && a->next) {
    a = a->next;
    --n;
  }
  if (n)
    return nullptr;
  else
    return a->value;
}


// Returns a poiter to the value associated with the key in [first, last)
// or nullptr if no such key exists.
Value* 
find(Object const* obj, char const* str)
{
  // There are no keys in an empty object.
  if (!obj->value)
    return nullptr;
  
  // Search the list for the key. Skip objects with non-terminal
  // keys (they shouldn't exist).
  while (obj) {
    Pair* p = obj->value;
    if (*p->key == str)
      return p->value;
    obj = obj->next;
  }
  return nullptr;
}


} // namespace


// -------------------------------------------------------------------------- //
//                           Value implementation

std::size_t
String::hash() const
{
  std::size_t h = 0;
  for (char const* i = first; i != last; ++i)
    boost::hash_combine(h, *i);
  return h;
}


// Returns the number of elements in the array. Note that this
// is a linear time operation.
int
Array::size() const
{
  return count(this);
}


// Return the value at index n, or nullptr if n is out of range.
Value* 
Array::operator[](int n) const
{
  return at(this, n);
}


// Returns the number of elements in the object. Note that this is
// a linear time operation.
int
Object::size() const
{
  return count(this);
}


// Return the value corresponding to the key, or nullptr if it
// does not exist.
Value* 
Object::operator[](char const* key) const
{
  return find(this, key);
}


// Return the value corresponding to the key, or nullptr if it
// does not exist.
Value* 
Object::operator[](std::string const& key) const
{
  return find(this, key.c_str());
}


// Return a map over the elements of this object. This supports
// efficient lookup of key value pairs.
auto
Object::map() const -> Map
{
  Map m;
  if (!empty()) {
    Object const* o = this;
    while (o) {
      Pair* p = o->value;
      m.insert({p->key, p->value});
      o = o->next;
    }
  }
  return m;
}


// -------------------------------------------------------------------------- //
//                               Streaming

namespace
{

void print_value(std::ostream&, Value const*);


// Print a sequence of characters in a terminal.
void
print_terminal(std::ostream& os, Terminal const* t)
{
  char const* iter = t->first;
  while (iter != t->last) {
    os << *iter;
    ++iter;
  }
}


inline void
print_string(std::ostream& os, String const* s)
{
  os << '"';
  print_terminal(os, s);
  os << '"';
}


inline void
print_pair(std::ostream& os, Pair const* p)
{
  print_value(os ,p->key);
  os << ':';
  print_value(os, p->value);
}


void
print_array(std::ostream& os, Array const* a)
{
  os << '[';
  if (a->value)
    while (a) {
      print_value(os, a->value);
      if (a->next)
        os << ',';
      a = a->next;
    }
  os << ']';
}


void
print_object(std::ostream& os, Object const* o)
{
  os << '{';
  if (o->value)
    while (o) {
      print_value(os, o->value);
      if (o->next)
        os << ',';
      o = o->next;
    }
  os << '}';
}


void
print_value(std::ostream& os, Value const* v)
{
  assert(v);
  switch (v->kind) {
  case identifier_value: return print_terminal(os, cast<Identifier>(v));
  case number_value: return print_terminal(os, cast<Number>(v));
  case string_value: return print_string(os, cast<String>(v));
  case pair_value: return print_pair(os, cast<Pair>(v));
  case array_value: return print_array(os, cast<Array>(v));
  case object_value: return print_object(os, cast<Object>(v));
  default: assert(false);
  }
}


} // namespace



std::ostream& 
operator<<(std::ostream& os, Value const& v)
{
  print_value(os, &v);
  return os;
}


// -------------------------------------------------------------------------- //
//                          Parsing


namespace
{

// Returns true if c is a character that can appear in
// an identifier: alphanumeric characters and '_'.
inline bool
isident(char c)
{
  return std::isalnum(c) || c == '_';
}


// Returns true if c is a character that can appear in
// a number: digits, '.', 'e', and 'E'. 
inline bool
isnum(char c)
{
  return std::isdigit(c) || c == '.' || c == 'e' || c == 'E';
}


inline void
eat_space(char const*& first, char const* last)
{
  while (first != last && std::isspace(*first))
    ++first;
}


inline bool
matches(char const*& first, char const* last, char c)
{
  if (first != last && *first == c) {
    ++first;
    return true;
  }
  return false;
}


struct Space_guard
{
  Space_guard(char const*& f, char const* l)
    : first(f), last(l)
  { 
    eat_space(first, last); 
  }

  ~Space_guard()
  {
    eat_space(first, last);
  }

  char const*& first;
  char const*  last;
};


// Empty values
static Array empty_array_(nullptr, nullptr);
static Object empty_object_(nullptr, nullptr);


} // namespace


Value*
Document::parse_value(char const*& first, char const* last)
{
  Space_guard ws(first, last);
  switch (*first) {
  case '-':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return parse_number(first, last);

  case '"': 
    return parse_string(first, last);
  
  case '[': 
    return parse_array(first, last);
  
  case '{': 
    return parse_object(first, last);
  
  default: 
    return parse_identifier(first, last);
  }
}


Identifier*
Document::parse_identifier(char const*& first, char const* last)
{
  char const* init = first++;
  while (first != last && isident(*first))
    ++first;
  return make_identifier(init, first);
}


Number*
Document::parse_number(char const*& first, char const* last)
{
  char const* init = first++;
  while (first != last && isnum(*first))
    ++first;
  return make_number(init, first);
}


String*
Document::parse_string(char const*& first, char const* last)
{
  char const* init = ++first; // Save the first character.
  while (first != last && *first != '"') {
    if (*first == '\\')
      ++first;
    ++first;
  }
  if (first == last)
    throw std::runtime_error("unterminated string");
  char const* end = first++;  // Save the last character
  return make_string(init, end);
}


String*
Document::parse_key(char const*& first, char const* last)
{
  Space_guard ws(first, last);
  if (*first != '"')
    throw std::runtime_error("ill-formed key");
  return parse_string(first, last);
}


Pair*
Document::parse_pair(char const*& first, char const* last)
{
  String* k = parse_key(first, last);
  if (!matches(first, last, ':'))
    throw std::runtime_error("ill-formed key/value pair");
  Value* v = parse_value(first, last);
  return make_pair(k, v);
}


Array*
Document::parse_array(char const*& first, char const* last)
{
  ++first; // Consume '['

  // Handle the empty array case.  
  eat_space(first, last);
  if (matches(first, last, ']'))
    return &empty_array_;

  Array* a = nullptr;
  Array* r = nullptr;
  do {
    // Parse the next value and update the array.
    Value* v = parse_value(first, last);
    a = make_array(v, a);
    if (!r)
      r = a;

  } while (matches(first, last, ','));

  if (!matches(first, last, ']'))
    throw std::runtime_error("ill-formed array");

  return r;
}


Object*
Document::parse_object(char const*& first, char const* last)
{
  ++first; // Consume '{'

  // Handle the empty object case.  
  eat_space(first, last);
  if (matches(first, last, '}'))
    return &empty_object_;

  Object* o = nullptr;
  Object* r = nullptr;
  do {
    // Parse the next value and update the object.
    Pair* p = parse_pair(first, last);
    o = make_object(p, o);
    if (!r)
      r = o;

  } while (matches(first, last, ','));

  if (!matches(first, last, '}'))
    throw std::runtime_error("ill-formed object");

  return r;
}


// -------------------------------------------------------------------------- //
//                         Simplified construction


// Make a command string from the C-string pairs in map.
//
// Note that this is not especially efficient.
std::string
make_object(std::initializer_list<std::pair<char const*, char const*>> map)
{
  std::string buf;
  for (auto i = map.begin(); i != map.end(); ++i) {
    buf += format(R"("{}":"{}")", i->first, i->second);
    if (std::next(i) != map.end())
      buf += ',';
  }
  return '{' + buf + '}';
}


// Make an array of the given strings.
//
// Note that this is not especially efficient.
std::string 
make_array(std::initializer_list<char const*> list)
{
  std::string buf;
  for (auto i = list.begin(); i != list.end(); ++i) {
    buf += *i;
    if (std::next(i) != list.end())
      buf += ',';
  }
  return '[' + buf + ']';
}


} // namespace json

} // namespace
