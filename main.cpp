
#include "fifo.hpp"

#include <iostream>
#include <cmath>

#include <sched.h>

int main()
{
  typedef Queue<uint64_t, 16*1024, false, false, false, false> Q;
  Q f;

	cpu_set_t	cur_mask;
	CPU_ZERO(&cur_mask);
	CPU_SET(2, &cur_mask);

	if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
    std::cerr << "Error: sched_setaffinity" << std::endl;
		return 1;
	}

  uint64_t ended = 0;
  for(uint64_t i = 1; i < 16*1024; ++i) {
    if ( f.enqueue( i * i ) != Q::SUCCESS ) {
      ended = i;
      break;
    }
  }


  uint64_t v;
  while(f.dequeue(v) != Q::BUFFER_EMPTY) {
    std::cout << v << " " << sqrt( (float)v ) << std::endl;
  }

  std::cout << "-------------- ended: " << ended << std::endl;
  return 0;
}
