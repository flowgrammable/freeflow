#ifndef FP_THREAD_HPP
#define FP_THREAD_HPP

#include "queue.hpp"
#include "application.hpp"

#include <pthread.h>
#include <vector>


namespace fp
{

class Application;
class Context;

#if __APPLE__
using Barrier_type = void*;
#else
using Barrier_type = pthread_barrier_t;
#endif


// The flowpath thread object. Wraps a posix thread.
class Thread
{
public:
  using Routine = void* (*)(void*);
  using Barrier = Barrier_type;
  using Attribute = pthread_attr_t;

  Thread();
  Thread(int, Routine, Attribute* = nullptr);
  Thread(int, Routine, Barrier*, Attribute* = nullptr);

  void run();
  void sync();
  int  halt();
  void assign(int, Routine, Attribute* = nullptr);
  void assign(int, Routine, Barrier*, Attribute* = nullptr);

  int 		 id_;
  Routine  work_;
  Barrier* barrier_;

private:
  pthread_t  thread_;
  Attribute* attr_;
};


// Provides an initializer/destroyer for thread barriers.
namespace Thread_barrier
{

#if __APPLE__
inline int
init(Thread::Barrier* barr, int num)
{
  return 0;
}


inline int
destroy(Thread::Barrier* barr)
{
  return 0;
}

inline int
wait(Thread::Barrier*)
{
  return 0;
}
#else
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

inline int
wait(Thread::Barrier* b)
{
  return pthread_barrier_wait(b);
}
#endif
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

// Disabling thread pool for now.
#if 0

// A thread pool instruction entry. Contains a function to be
// called, the arguments needed.
struct Task
{
  enum Target { CONFIG, PIPELINE, PORTS };
  using Arg = void*;

  Task(Target func, Arg arg)
    : func_(func), arg_(arg)
  { }

  ~Task()
  { }

  Target func() const { return func_; }
  Arg 	 arg()  const { return arg_; }

  Target func_;
  Arg 	 arg_;
};


// The thread pool work function.
void* Thread_pool_routine(void*);

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

  Thread::Routine pool_work_ = Thread_pool_routine;

  // Thread attribute variable.
  Thread::Attribute attr_;

  // FIXME: Apple does not support explicitly binding
  // a thread to processor.
#if ! __APPLE__
  // CPU core mask variable.
  cpu_set_t cpu_set_;
#endif

  // The thread pool.
  std::vector<Thread*> pool_;
  // The thread pool barrier.
  Thread::Barrier barr_;

  // The thread pool work queue.
  Input_queue 	input_;

  // Allocates the pool based on size.
  void 	alloc_pool();
};

extern Thread_pool thread_pool;

#endif // end thread pool

} // end namespace fp

#endif
