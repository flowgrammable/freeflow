#include "queue.hpp"


namespace fp
{

// The flowpath mutex constructor.
Mutex::Mutex()
{
  if (pthread_mutex_init(&mutex_, nullptr))
    throw("failed to create mutex");
}


// The flowpath mutex destructor.
Mutex::~Mutex()
{
  pthread_mutex_destroy(&mutex_);
}


// Locks the mutex.
void
Mutex::lock()
{
  pthread_mutex_lock(&mutex_);
}


// Unlocks the mutex.
void
Mutex::unlock()
{
  pthread_mutex_unlock(&mutex_);
}

} // end namespace fp
