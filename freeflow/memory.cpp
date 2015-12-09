
#include "memory.hpp"

#include <iostream>

namespace ff
{

namespace
{

// The global default allocaor.
Allocator default_alloc_;

} // namesapce


// Returns the default allocator.
Allocator&
default_allocator()
{
  return default_alloc_;
}


Sequential_allocator::~Sequential_allocator()
{
  Block* p = head;
  while (p) {
    Block* q = p->next;
    p->~Block();
    ::operator delete(p);
    p = q;
  }
}


// Allocate n bytes of memory from the current block.
void*
Sequential_allocator::allocate(int n)
{
  int pos = sizeof(Block) + offset;

  // Make sure we have the memory we need. Be sure that writes
  // to an object of size n do not land on the boundary of
  // the block.
  if (!head)
    allocate_block();
  else if (pos + n >= size - (int)sizeof(Block))
    allocate_block();

  // Compute the next address and bump the pointer.
  char* base = reinterpret_cast<char*>(tail);
  char* ptr = base + pos;
  offset += n;

  return ptr;
}


// Allocate a new block.
Sequential_allocator::Block*
Sequential_allocator::allocate_block()
{
  void* p = ::operator new(size);
  Block* b = new (p) Block { nullptr };

  // Link the node into the list.
  if (!head)
    head = b;
  else
    tail->next = b;
  tail = b;

  // Reset the offset.
  offset = 0;

  return b;
}


int
Sequential_allocator::allocated() const
{
  int alloc = 0;
  Block* p = head;
  while (p != tail) {
    alloc += size - sizeof(Block);
    p = p->next;
  }
  alloc += offset - sizeof(Block);
  return alloc;
}



} // namespace ff
