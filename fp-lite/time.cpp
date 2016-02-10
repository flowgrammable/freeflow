#include "time.hpp"

#include <chrono>

namespace fp
{

using namespace std::chrono;


inline Timestamp
Time::current() const
{
	return (duration_cast<::milliseconds>(std::chrono::steady_clock::now()).count());
}


}	// namespace fp
