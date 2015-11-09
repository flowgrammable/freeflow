#include "thread.hpp"

#include <iostream>
#include <unistd.h>

// Simple thread work stub.
static void* 
report(void* args)
{
  int id = *((int*)args);
  std::cerr << "Hello from thread " << id << '\n';
  return 0;
}


int
main(int argc, char** argv)
{
  fp::Thread* thread = new fp::Thread(1, report);
  std::cerr << "Created thread with id 1\n";
  thread->run();
  std::cerr << "Thread started...\n";
  sleep(2);
  thread->halt();
  std::cerr << "Thread stopped.\n";
  return 0;
}