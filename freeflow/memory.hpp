
#ifndef FREEFLOW_MEMORY_HPP
#define FREEFLOW_MEMORY_HPP

#include <cassert>
#include <list>
#include <new>
#include <memory>

namespace ff
{

// The allocator class is a simple polymorphic wrapper around
// a block allocator.
class Allocator
{
public:
  virtual ~Allocator() { }
  
  virtual void* allocate(int n)
  {
    return ::operator new(n);
  }

  virtual void deallocate(void* n)
  {
    return ::operator delete(n);
  }
};


Allocator& default_allocator();


// The sequenital allocator maintians a list of n-byte blocks (where n is 
// a multiple of page size), and simply allocates memory as an offset 
// within that pointer. When the offset would exceed the current block, 
// we allocate a new block.
//
// Note that block allocation is done using a basic allocator.
//
// All memory is released when the allocator is destroyed.
//
// This type of allocation is very prone to fragmentation, because 
// effort is made to recycle deallocated regions of memory. In
// fact, deallocation does nothing at all.
//
// This allocator is particularly effective, when a program would 
// invoke a sequence of allocations for an object that is destroyed
// shortly thereafter. Examples include string building and AST 
// construction during parsing.
//
//    void f()
//    {
//      Sequential_allocator alloc;
//
//      json::Document doc(alloc);
//      json::Value* v = json::parse(str);
//
//      // Act on the parsed value
//
//    } // All memory automatically reclaimed here.
//
// In these contexts, depending on the type of objects allocated,
// it may not be necessary to explicitly destory objects. This
// is the case when the type of object allocated is trivially
// destructible (e.g., POD types).
class Sequential_allocator : public Allocator
{
  struct Block
  {
    Block* next;
  };

  static constexpr int page_size = 4096; // FIXME: use getpagesize.

public:
  Sequential_allocator(int n)
    : size(n), offset(0), head(), tail()
  { }

  ~Sequential_allocator();

  void* allocate(int);
  void deallocate(void*);

  int allocated() const;

private:
  Block* allocate_block();

  int    size;
  int    offset;
  Block* head;
  Block* tail;
};


// Deallocate memory pointed to by p. Becuase no mmeory
// is recycled, this is a no-op.
inline void
Sequential_allocator::deallocate(void*)
{
}


// A buffer allocatr allocates memory from a user-provided buffer.
// A buffer allocator is a restricted form of sequential allocator,
// in that it does not grow when more memory is needed. Allocations in 
// excess of the user-provided buffer result in undefined behavior.
//
// Note deallocation is a no-op for a buffered allocator.
class Buffer_allocator : public Allocator
{
public:
  Buffer_allocator(char* buf, int n)
    : buf(buf), size(n), offset(0)
  { }

  void* allocate(int);
  void deallocate(void*);

  char* buf;
  int   size;
  int   offset;
};


inline void* 
Buffer_allocator::allocate(int n)
{
  assert(offset + n <= size);
  char* p = buf + offset;
  offset += n;
  return p;
}

inline void
Buffer_allocator::deallocate(void*)
{
}


} // namespace ff

#endif
