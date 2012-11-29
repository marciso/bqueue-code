

//#define _GNU_SOURCE
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include "fifo2.hpp"
#include <cmath>

int main()
{
  typedef struct queue<> queue_t;
	uint64_t	e=0,i;
	cpu_set_t	cur_mask;
	uint64_t value;

  queue_t q;

	CPU_ZERO(&cur_mask);
	CPU_SET(0, &cur_mask);

	if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
		perror("Error: sched_setaffinity");
		return 1;
	}

  uint64_t s = 8*1024;
  for(i=1;i<s;++i){
    if( q.enqueue(i*i) != queue_t::SUCCESS ) {
      e = i;
      break;
    }
  }
  //queue.enqueue(0) ;

  while( q.dequeue(&value) == queue_t::SUCCESS ) {
    std::cout << value << " " << sqrt( (float)value ) << std::endl;
  }

  std::cout << "------- end: " << e << ", size: " << s << std::endl;
  return 0;
}
