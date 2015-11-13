// An example application showcasing the ability to set thread affinity
// before and during a thread's life time. The example defaults to create
// one thread per logical core found on the system.

#define _GNU_SOURCE
#include <sched.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>


// Global barrier variable shared by all threads for synchronization.
pthread_barrier_t barrier;

// Number of processors available in the system.
int num_proc;


// Convenience method for synchronizing threads.
inline void
sync()
{
  pthread_barrier_wait(&barrier);
}


// Thread work function. Takes an integer, passed as a void*, 
// representing the thread's logical ID. After reporting the
// logical and physical ID, as well as the logical core it is
// attached to, it moves to a different core and reports again.
void* 
work(void* args)
{
  // The logical thread ID.
  int id = (*(int*)args);  
  fprintf(stderr, "[%d]: waiting...\n", id);
  
  // Have all threads line up after being created.
  sync();
  
  // Print the logical and physical ID, and the logical core ID.
  fprintf(stderr, "[%d]: TID:%lu CPU:%d\n", id, pthread_self(), sched_getcpu());
  sync();

  // Have the 'master' thread announce a change.
  if (!id)
    fprintf(stderr, "Change places!\n");
  sync();
  
  // Create a new CPU core mask.
  cpu_set_t cpu_set;
  
  // Initialize the core mask.
  CPU_ZERO(&cpu_set);
  
  // Set the mask.
  CPU_SET((id+1) % num_proc, &cpu_set);
  
  // Set the core you want the thread to be running on. This is a different
  // call than the one from 'main', since the thread is already running.
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
  
  // Report the logical and physical thread ID, and the logical core ID again.
  fprintf(stderr, "[%d]: TID:%lu CPU:%d\n", id, pthread_self(), sched_getcpu());
  
  return 0;
}


// Driver for thread affinity example applicaiton.
int
main(int argc, char** argv)
{  
  // Get the number of logical cores available on the system.
  num_proc = sysconf(_SC_NPROCESSORS_ONLN);
  fprintf(stderr, "Number of processors: %d\n", num_proc);
  
  // Create a thread pool, one thread per logical core.
  pthread_t pool[num_proc];
  
  // Thread attribute variable.
  pthread_attr_t attr;
  
  // CPU core mask variable.
  cpu_set_t cpu_set;
  
  // Initialize the attribute var.
  pthread_attr_init(&attr);
  
  // Initialize the thread barrier variable. If this fails, exit.
  if (pthread_barrier_init(&barrier, NULL, num_proc)) {
    fprintf(stderr, "err: barrier init failed\n");
    return -1;
  }
  
  // Create a small buffer to keep track of the logical thread IDs.
  // We have to do this instead of passing the address of 'i' in the
  // following for-loop because by the time the thread reads that 
  // value, it as already been incremented.
  int* ids = (int*)malloc(num_proc * sizeof(int));
  for (int i = 0; i < num_proc; i++) {
    ids[i] = i;
    
    // Clear the CPU core mask.
    CPU_ZERO(&cpu_set);
    
    // Set the CPU core mask.
    CPU_SET(i, &cpu_set);
    
    // Set the core you want the thread to run on using a thread
    // attribute variable. We do this to ensure the thread starts
    // on the core we want it to be on.
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpu_set);
    
    // Create the thread.
    pthread_create(&pool[i], &attr, work, (void*)&ids[i]);
  }

  // Main process will wait until threads start to finish and return.
  for (int i = 0; i < num_proc; i++) {
    pthread_join(pool[i], NULL);
  }

  // Cleanup
  free(ids);

  return 0;
}