

//#define _GNU_SOURCE
#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include "fifo2.hpp"
#include <cmath>

int main()
{
	uint64_t	e=0,i;
	cpu_set_t	cur_mask;
	ELEMENT_TYPE	value;
  struct queue queue;

	CPU_ZERO(&cur_mask);
	CPU_SET(0, &cur_mask);

	if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
		perror("Error: sched_setaffinity");
		return 1;
	}

  queue.init();

  uint64_t s = 8*1024-1;
  for(i=1;i<s;++i){
    if( queue.enqueue(i*i) != queue::SUCCESS ) {
      e = i;
      break;
    }
  }
  //queue.enqueue(0) ;

  while( queue.dequeue(&value) == queue::SUCCESS ) {
    std::cout << value << " " << sqrt( (float)value ) << std::endl;
  }

  std::cout << "------- end: " << e << ", size: " << s << std::endl;
  return 0;
}
