#include "thread.hpp"

#include <pthread.h>
#include <cassert>
#include <algorithm>

namespace fp
{


extern Thread_pool thread_pool;


// Constructs a new thread object with no ID, work function, or barrier.
// This thread must be initialized with `assign` before it can be used.
Thread::Thread()
	: Thread(-1, nullptr)
{ }


// Constructs a new thread object with the given ID and work function.
// This thread does not use a barrier to synchronize.
Thread::Thread(int id, Work_fn work)
	: id_(id), work_(work), barrier_(nullptr)
{ }


// Constructs a new thread object with the given ID, work function, and
// synchronization barrier.
Thread::Thread(int id, Work_fn work, Barrier* barrier)
	: id_(id), work_(work), barrier_(barrier)
{ }


// Starts the thread with its work function and passes its ID as arg.
void
Thread::run()
{
	assert(work_);
	// TODO: create the underlying pthread. Use the local
	// work_fn member as the func ptr and the local ID as
	// the argument.
	if (pthread_create(&thread_, nullptr, work_, &id_) != 0)
		throw("failed to create thread");
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
		throw("failed to join thread");
	return 0;
}


// (Re)Assigns an ID and work function to the thread. Calling this during
// thread execution will cause undefined behavior.
void
Thread::assign(int id, Work_fn work)
{
	assign(id, work, nullptr);
}


// (Re)Assigns an ID, work function, and barrier to the thread. Calling this
// during thread execution will cause undefined behavior.
void
Thread::assign(int id, Work_fn work, Barrier* barr)
{
	id_ = id;
	work_ = work;
	barrier_ = barr;
}


// The default flowpath thread pool ctor.
Thread_pool::Thread_pool(int size, bool sync, Thread::Work_fn work)
	: size_(size), sync_(sync), running_(false), work_(work)
{ 
	alloc_pool();
}

// The flowpath thread pool dtor.
Thread_pool::~Thread_pool()
{ }


// Enqueues the item in the work queue for processing.
void
Thread_pool::assign(Task item)
{ 
	input_.enqueue(item);
}


// Dequeues the next item from the work queue for processing.
bool
Thread_pool::request(Task& ret)
{ 
	return input_.dequeue(ret);
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
	alloc_pool();
	start();
}


// Allocate the thread pool.
void
Thread_pool::alloc_pool()
{	
	pool_ = new Thread*[size_];
	if (sync_) {
		Thread_barrier::init(&barr_, size_);
		for (int i = 0; i < size_; i++)
			pool_[i] = new Thread(i, work_, &barr_);
	}
	else {
		for (int i = 0; i < size_; i++)
			pool_[i] = new Thread(i, work_);
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


} // end namespace fp