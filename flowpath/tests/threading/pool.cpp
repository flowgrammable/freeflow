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
static fp::Thread_pool pool(5, true, req_work);

// Data for the thread pool.
static int t[] = {0,1,2,3,4,5,6,7,8,9};

// Work function definition.
//
// A simple thread work stub. Retrieves a work item from the thread pool
// and prints the value.
static void* 
req_work(void* args)
{
  // Figure out who I am.
  int id = *((int*)args);
  fp::Task* tsk = nullptr;

  // Local counter for work done.
  int my_work = 0;

  // Buffer messages into strings to make them print better.
  std::string msg;
  msg = std::string("[thread:" + std::to_string(id) + "] started.\n");
  std::cout << msg;
  
  // Wait until everyone is ready.
  sync();

  // Start processing.
  while (pool.running()) {
    // Request a work item from the pool.
    if (pool.request(*tsk)) {
      // Increment local work counter.
      ++my_work;
      // Execute the task.
      tsk->execute();
      // Delete the task.
      delete tsk;
      // Sleep for a second. Since there isn't much to do here we need
      // to allow other threads to pick up some work.
      sleep(std::rand() % 3);
    }
  }
  msg = std::string("[thread:" + std::to_string(id) + "] stopped. Processed " + 
    std::to_string(my_work) + " items.\n");
  std::cout << msg;
  sync();
  return 0;
}


// Adds work to the thread pool.
static void*
add_work(void* args)
{
  // Assign work to the pool.
  for (int i = 0; i < 80; i++) {
    pool.assign(fp::Task(work, &t[i % 10]));
  }

  return 0;
}


// The task work function.
static void*
work(void* arg)
{
  fp::Task* tsk = (fp::Task*)arg;
  // Report value.
  std::string msg = std::string("Found item '" + 
    std::to_string(*((int*)tsk->arg_)) + "'.\n");
  std::cout << msg;
  return 0;
}

// Thread pool test. Assigns work to the thread pool at a semi-random
// rate.
int
main(int argc, char** argv)
{
  // Setup and start the add_work thread.
  fp::Thread* add_worker = new fp::Thread(0, add_work);
  std::cout << "[master] Starting the add_work thread...\n";
  add_worker->run();
  
  // Start the thread pool.
  std::cout << "[master] Starting the work pool...\n";
  pool.start();
  
  // Let stuff happen.
  sleep(4);
  
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
  
  return 0;
}