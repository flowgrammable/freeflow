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


// Synchronizes threads sharing the same barrier.
void
Thread::sync()
{
	assert(barrier_);
	Thread_barrier::wait(barrier_);
}


// Stops the thread work function and returns the return value from
// the thread work.
int
Thread::halt()
{
	int* ret = new int();
	if (pthread_join(thread_, (void**)&ret) != 0)
		throw(std::string("failed to join thread"));
	return *ret;
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


// Installs the application in the thread pool.
void
Thread_pool::install(Application* app)
{
	if (!app_)
		app_ = app;
	else
		throw std::string("Application '" + app_->name() + "' already installed in thread pool");
}


// Removes the application from the thread pool.
void
Thread_pool::uninstall()
{
	if (app_) {
		switch (app_->state()) {
			case Application::RUNNING:
			case Application::WAITING:
				throw std::string("Application '" + app_->name() + "' is running");
			break;
			case Application::READY:
			case Application::NEW:
			case Application::STOPPED:
				app_ = nullptr;
			break;
		}
	}
	else
		throw std::string("No application currently installed in thread pool");
}


bool
Thread_pool::has_work()
{
	return input_.size();
}


// Resizes the thread pool.
void
Thread_pool::resize(int s)
{
	// Stop processing.
	stop();
	// Set new size.
	size_ = s;
	// Empty the thread pool.
	pool_.clear();
	// Destroy barrier, if synced, and attribute.
	if (sync_)
		Thread_barrier::destroy(&barr_);
	Thread_attribute::destroy(&attr_);
	// Reallocate the pool with new size.
	alloc_pool();
	// Start the pool again.
	start();
}


// Allocate the thread pool.
void
Thread_pool::alloc_pool()
{
	// Create thread attribute.
	Thread_attribute::init(&attr_);
	if (sync_) {
		Thread_barrier::init(&barr_, size_);
		for (int i = 0; i < size_; i++) {
      #if !__APPLE__
	    // Clear the CPU core mask.
	    CPU_ZERO(&cpu_set_);
	    // Set the CPU core mask.
	    CPU_SET((i + num_proc_) % num_proc_, &cpu_set_);

	    // Set the core you want the thread to run on using a thread
	    // attribute variable. We do this to ensure the thread starts
	    // on the core we want it to be on.
	    pthread_attr_setaffinity_np(&attr_, sizeof(cpu_set_t), &cpu_set_);
      #endif

			// Create the thread.
			pool_.push_back(new Thread(i, pool_work_, &barr_, &attr_));
		}
	}
	else {
		for (int i = 0; i < size_; i++) {
      #if !__APPLE__
	    // Clear the CPU core mask.
	    CPU_ZERO(&cpu_set_);
	    // Set the CPU core mask.
	    CPU_SET((i + num_proc_) % num_proc_, &cpu_set_);

	    // Set the core you want the thread to run on using a thread
	    // attribute variable. We do this to ensure the thread starts
	    // on the core we want it to be on.
	    pthread_attr_setaffinity_np(&attr_, sizeof(cpu_set_t), &cpu_set_);
      #endif

			// Create the thread.
			pool_.push_back(new Thread(i, pool_work_, &attr_));
		}
	}
}


// Starts the thread pool processing.
void
Thread_pool::start()
{
	app_->start();
	for (Thread* thread : pool_)
		thread->run();
}


// Stops the thread pool processing.
void
Thread_pool::stop()
{
	app_->stop();
	for (Thread* thread : pool_)
		thread->halt();
}


// Reports the state of the thread pool.
bool
Thread_pool::running()
{
	return app_->state() == Application::RUNNING;
}


// Returns the currently installed application.
Application*
Thread_pool::app()
{
	return app_;
}


// The thread pool work function. This is what the thread pool
// threads execute; request tasks and execute them.
void*
Thread_pool_work_fn(void* args)
{
	// Figure out who I am.
	//int id = *((int*)args);
	fp::Task* tsk = nullptr;

	while (thread_pool.running()) {
		// Get the next task to be executed.
		if ((tsk = thread_pool.request())) {
			// Execute the installed application function with arg.
			thread_pool.app()->lib().exec(tsk->func(),tsk->arg());
			delete tsk;
		}
	}
	return 0;
}


} // end namespace fp
