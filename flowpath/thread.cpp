#include "thread.hpp"

#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <cassert>
#include <algorithm>
#include <string>

namespace fp
{


extern Thread_pool thread_pool;
extern Application_


// Constructs a new thread object with no ID, work function, or barrier.
// This thread must be initialized with `assign` before it can be used.
Thread::Thread()
	: Thread(-1, nullptr)
{ }


// Constructs a new thread object with the given ID and work function.
// This thread does not use a barrier to synchronize.
Thread::Thread(int id, Work_fn work, Attribute* attr)
	: id_(id), work_(work), barrier_(nullptr), attr_(attr)
{ }


// Constructs a new thread object with the given ID, work function, and
// synchronization barrier.
Thread::Thread(int id, Work_fn work, Barrier* barrier, Attribute* attr)
	: id_(id), work_(work), barrier_(barrier), attr_(attr)
{ }


// Starts the thread with its work function and passes its ID as arg.
void
Thread::run()
{
	assert(work_);
	// TODO: create the underlying pthread. Use the local
	// work_fn member as the func ptr and the local ID as
	// the argument.
	int res;
	if ((res = pthread_create(&thread_, (const Thread::Attribute*)attr_, work_, &id_)) != 0){
		errno = res;
		perror(std::string("failed to create thread " + std::to_string(id_)).c_str());
	}
}


// Synchronizes on the given barrier.
void
Thread::sync()
{
	assert(barrier_);
	pthread_barrier_wait(barrier_);
}


// Stops the thread work function and returns the return value from
// the thread work.
int
Thread::halt()
{
	int* ret = new int();
	if (pthread_join(thread_, (void**)&ret) != 0)
		throw(std::string("failed to join thread"));
	return 0;
}


// (Re)Assigns an ID and work function to the thread. Calling this during
// thread execution will cause undefined behavior.
void
Thread::assign(int id, Work_fn work, Attribute* attr)
{
	assign(id, work, nullptr, attr);
}


// (Re)Assigns an ID, work function, and barrier to the thread. Calling this
// during thread execution will cause undefined behavior.
void
Thread::assign(int id, Work_fn work, Barrier* barr, Attribute* attr)
{
	id_ = id;
	work_ = work;
	barrier_ = barr;
	attr_ = attr;
}


// The default flowpath thread pool ctor.
Thread_pool::Thread_pool(int size, bool sync)
	: size_(size), sync_(sync), running_(false)
{ 
	num_proc_ = sysconf(_SC_NPROCESSORS_ONLN);
	alloc_pool();
}


// The flowpath thread pool dtor.
Thread_pool::~Thread_pool()
{ }


// Enqueues the item in the work queue for processing.
void
Thread_pool::assign(Task* item)
{ 
	input_.enqueue(item);
}


// Dequeues the next item from the work queue for processing.
Task*
Thread_pool::request()
{ 
	return input_.dequeue();
}


bool
Thread_pool::has_work()
{ 
	return input_.size();
}


// Checks if there is any work to be done.
bool
Thread_pool::has_work()
{ 
	return input_.size();
}


// Resizes the thread pool.
void
Thread_pool::resize(int s)
{
	stop();
	size_ = s;
	delete[] pool_;
	if (sync_)
		Thread_barrier::destroy(&barr_);
	Thread_attribute::destroy(&attr_);
	alloc_pool();
	start();
}


// Allocate the thread pool.
void
Thread_pool::alloc_pool()
{	
	Thread_attribute::init(&attr_);
	pool_ = new Thread*[size_];
	if (sync_) {
		Thread_barrier::init(&barr_, size_);
		for (int i = 0; i < size_; i++) {
	    // Clear the CPU core mask.
	    CPU_ZERO(&cpu_set_);
	    
	    // Set the CPU core mask.
	    CPU_SET((i + num_proc_) % num_proc_, &cpu_set_);
	    
	    // Set the core you want the thread to run on using a thread
	    // attribute variable. We do this to ensure the thread starts
	    // on the core we want it to be on.
	    pthread_attr_setaffinity_np(&attr_, sizeof(cpu_set_t), &cpu_set_);
			
			// Create the thread.
			pool_[i] = new Thread(i, Thread_pool_work_fn, &barr_, &attr_);
		}
	}
	else {
		for (int i = 0; i < size_; i++) {
	    // Clear the CPU core mask.
	    CPU_ZERO(&cpu_set_);
	    
	    // Set the CPU core mask.
	    CPU_SET((i + num_proc_) % num_proc_, &cpu_set_);
	    
	    // Set the core you want the thread to run on using a thread
	    // attribute variable. We do this to ensure the thread starts
	    // on the core we want it to be on.
	    pthread_attr_setaffinity_np(&attr_, sizeof(cpu_set_t), &cpu_set_);
			
			// Create the thread.			
			pool_[i] = new Thread(i, Thread_pool_work_fn, &attr_);
		}
	}	
}


// Starts the thread pool processing.
void
Thread_pool::start()
{	
	running_ = true;
	for (int i = 0; i < size_; i++)
		pool_[i]->run();	
}


// Stops the thread pool processing.
void
Thread_pool::stop()
{
	running_ = false;
	for (int i = 0; i < size_; i++)
		pool_[i]->halt();
}


// Reports the state of the thread pool.
bool
Thread_pool::running()
{
	return running_;
}


// The thread pool work function. This is what the thread pool
// threads execute; request tasks and execute them.
static void*
Thread_pool_work_fn(void* args)
{
	// Figure out who I am.
	int id = *((int*)args);
	fp::Task* tsk = nullptr;
	
	while (thread_pool.running()) {
		// Get the next task to be executed.
		if ((tsk = thread_pool.request())) {
			// Execute the application target function with arg.
			application_libraries.[tsk->app()]->exec(tsk->target())(tsk->arg());
			delete tsk;
		}
	}
	return 0;
}


} // end namespace fp