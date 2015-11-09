#ifndef FP_QUEUE_HPP
#define FP_QUEUE_HPP

#include <queue>
#include <pthread.h>

namespace fp
{


// The flowpath mutex object. Wraps a pthread mutex.
class Mutex
{
public:
  Mutex();
  ~Mutex();

  void lock();
  void unlock();

private:
  pthread_mutex_t mutex_;
};


// The flowpath locking queue. Uses a mutex to ensure atomic addition
// and/or retrieval of queued items.
template <typename T>
class Locking_queue
{
public:
  Locking_queue();
  ~Locking_queue();

  void enqueue(T&);
  bool dequeue(T&);

  T get();

  int size() const { return queue_.size(); }
  bool empty() const { return queue_.empty(); }
  
private:
  std::queue<T> queue_;
  Mutex mutex_;
};


template <typename T>
Locking_queue<T>::Locking_queue()
  : queue_(), mutex_()
{ }


template <typename T>
Locking_queue<T>::~Locking_queue()
{
  while (!queue_.empty())
    queue_.pop();
}


template <typename T>
void
Locking_queue<T>::enqueue(T& v)
{
  mutex_.lock();
  queue_.push(v);
  mutex_.unlock();
}


template <typename T>
bool
Locking_queue<T>::dequeue(T& ret)
{
  bool res = false;
  mutex_.lock();
  if (!queue_.empty()) {
    ret = queue_.front();
    queue_.pop();
    res = true;
  }
  mutex_.unlock();
  return res;
}


} // end namespace fp

#endif