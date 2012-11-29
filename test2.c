
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include "fifo.h"
#include <math.h>

int main()
{
	uint64_t	e=0,i;
	cpu_set_t	cur_mask;
	ELEMENT_TYPE	value;
  struct queue_t queue;

	CPU_ZERO(&cur_mask);
	CPU_SET(0, &cur_mask);

	if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
		perror("Error: sched_setaffinity");
		return 1;
	}

  queue_init(&queue);

  uint64_t s = 8*1024-1;
  for(i=1;i<s;++i){
    if( enqueue(&queue, i*i) != SUCCESS ) {
      printf("enqueue done %" PRIu64 "\n", i);
      e = i;
      break;
    }
  }
  //enqueue(&queue, 0) ;

  while( dequeue(&queue, &value) == SUCCESS ) {
    printf("%" PRIu64 " %f\n", value, sqrt( (float)value ) );
  }

  printf("------- end: %" PRIu64 ", size: %" PRIu64 "\n", e, s);
  return 0;
}
