#ifndef FP_THREAD_HPP
#define FP_THREAD_HPP 

#include <pthread.h>
#include <vector>
#include "queue.hpp"

namespace fp
{


// The flowpath thread object. Wraps a posix thread.
class Thread
{
public:
	using Work_fn = void* (*)(void*);
	using Barrier = pthread_barrier_t;

	Thread();
	Thread(int, Work_fn);
	Thread(int, Work_fn, Barrier*);

	void run();
	void sync();
	int  halt();
	void assign(int, Work_fn);
	void assign(int, Work_fn, Barrier*);

	int 		id_;
	Work_fn work_;
	Barrier* barrier_;
	
private:
	pthread_t thread_;
};


// Provides an initializer for thread barriers.
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


// A thread pool instruction entry. Contains a function to be
// called, the arguments needed, and the resulting value.
struct Task
{
	using Func = void* (*)(void*);
	using Arg = void*;

	Task(Func f, Arg a)
		: func_(f), arg_(a)
	{ }

	void execute() { func_(arg_); }

	Func 	func_;
	Arg 	arg_;
};


// The flowpath thread pool. Contains a user defined
// number of threads to execute tasks allocated to the
// thread pool work queue.
class Thread_pool
{
	using Input_queue = Locking_queue<Task>;
public:
	Thread_pool(int, bool, Thread::Work_fn);
	~Thread_pool();

	void 	assign(Task);
	bool 	request(Task&);

	bool 	has_work();
	void 	resize(int);

	void 	start();
	void 	stop();

	bool running();

	static void* work_fn(void*);

private:
	int size_;
	bool sync_;
	bool running_;

	Thread** pool_;
	Thread::Barrier barr_;
	Thread::Work_fn work_;
	
	Input_queue 	input_;

	void 	alloc_pool();
};


} // end namespace fp

#endif