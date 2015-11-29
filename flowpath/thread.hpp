#ifndef FP_THREAD_HPP
#define FP_THREAD_HPP 

#include <pthread.h>
#include <vector>

#include "queue.hpp"
#include "application.hpp"

namespace fp
{

struct Application;


// The flowpath thread object. Wraps a posix thread.
class Thread
{
public:
	using Work_fn = void* (*)(void*);
	using Barrier = pthread_barrier_t;
	using Attribute = pthread_attr_t;

	Thread();
	Thread(int, Work_fn, Attribute* = nullptr);
	Thread(int, Work_fn, Barrier*, Attribute* = nullptr);

	void run();
	void sync();
	int  halt();
	void assign(int, Work_fn, Attribute* = nullptr);
	void assign(int, Work_fn, Barrier*, Attribute* = nullptr);

	int 		id_;
	Work_fn work_;
	Barrier* barrier_;
	
private:
	pthread_t thread_;
	Attribute* attr_;
};


// Provides an initializer/destroyer for thread barriers.
namespace Thread_barrier
{

inline int 
init(Thread::Barrier* barr, int num)
{
	return pthread_barrier_init(barr, nullptr, num);	
}


inline int
destroy(Thread::Barrier* barr)
{
	return pthread_barrier_destroy(barr);
}

} // end namespace Thread_barrier


// Provides an initializer/destoryerfor thread attributes.
namespace Thread_attribute
{

inline int
init(Thread::Attribute* attr)
{
	return pthread_attr_init(attr);
}

inline int
destroy(Thread::Attribute* attr)
{
	return pthread_attr_destroy(attr);
}

} // end namespace Thread_attribute


// A thread pool instruction entry. Contains a function to be
// called, the arguments needed, and the resulting value.
struct Task
{
	using Label = std::string;
	using Arg = void*;

	Task(Label func, Arg arg)
		: func_(func), arg_(arg)
	{ }

	Label func() const { return func_; }
	Arg 	arg() const { return arg_; }

	Label app_;
	Label func_;
	Arg 	arg_;
};


// The thread pool work function.
void* Thread_pool_work_fn(void*);

// The flowpath thread pool. Contains a user defined
// number of threads to execute tasks allocated to the
// thread pool work queue.
class Thread_pool
{
	using Input_queue = Locking_queue<Task*>;
public:
	Thread_pool(int, bool);
	~Thread_pool();

	void 	assign(Task*);
	Task*	request();

	void 	install(Application*);
	void	uninstall();

	bool 	has_work();
	void 	resize(int);

	void 	start();
	void 	stop();

	bool running();
	Application* app();

private:
	// Size of the thread pool.
	int size_;
	// Flag indicating the use of synchronization.
	bool sync_;
	// Flag indicating state of the thread pool.
	bool running_;
	// Number of processing cores available.
	int num_proc_;
	// The application currently installed.
	Application* app_;

	Thread::Work_fn pool_work_ = Thread_pool_work_fn;

  // Thread attribute variable.
  Thread::Attribute attr_;  
  // CPU core mask variable.
  cpu_set_t cpu_set_;

  // The thread pool.
	Thread** pool_;
	// The thread pool barrier.
	Thread::Barrier barr_;
	
	// The thread pool work queue.
	Input_queue 	input_;

	// Allocates the pool based on size.
	void 	alloc_pool();
};

extern Thread_pool thread_pool;

} // end namespace fp

#endif