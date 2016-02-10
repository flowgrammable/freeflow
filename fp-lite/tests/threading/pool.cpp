#include "thread.hpp"

#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>

// Work function declaration.
static void* work(void*);
// Add work function declaration.
static void* add_work(void*);
// Request work function declaration.
static void* req_work(void*);

// The thread pool.
static fp::Thread_pool pool(2, true);

// Data for the thread pool.
static int t[] = {0,1,2,3,4,5,6,7,8,9};


// Adds work to the thread pool.
static void*
add_work(void* args)
{
  // Assign work to the pool.
  for (int i = 0; i < 1000000; i++) {
    pool.assign(new fp::Task(work, &t[i % 10]));
  }
  return 0;
}


// The task work function.
static void*
work(void* arg)
{
  int val = *((int*)arg);
  val += val;
  return 0;
}


// Thread pool test. Assigns work to the thread pool at a semi-random
// rate.
int
main(int argc, char** argv)
{
  try
  {
    // Setup and start the add_work thread.
    fp::Thread* add_worker = new fp::Thread(0, add_work);
    std::cout << "[master] Starting the add_work thread...\n";
    add_worker->run();
    
    // Start the thread pool.
    std::cout << "[master] Starting the work pool...\n";
    pool.start();
    
    // Resize the work pool.
    std::cout << "[master] Resizing work pool...\n";
    pool.resize(10);
    std::cout << "[master] Resized work pool.\n";
    
    // Join the add_work thread.
    std::cout << "[master] Waiting for add_work thread...\n";
    add_worker->halt();
    std::cout << "[master] Add_work thread joined.\n";
    
    // Wait until the pool has no more work in the queue.
    std::cout << "[master] Waiting for work pool to be empty...\n";
    while (pool.has_work())
      sleep(1);
    std::cout << "[master] Work pool empty.\n";

    // Stop the work pool.
    std::cout << "[master] Stopping work pool...\n";
    pool.stop();
    std::cout << "[master] Stopped work pool.\n";
  }
  catch(std::string msg)
  {
    std::cerr << "Error: " << msg << '\n';
    return -1;
  }
  catch(...)
  {
    perror("Unknown fatal error");
  }
  return 0;
}