/*
 *  B-Queue -- An efficient and practical queueing for fast core-to-core
 *             communication
 *
 *  Copyright (C) 2011 Junchang Wang <junchang.wang@gmail.com>
 *
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include "fifo.h"

#if defined(FIFO_DEBUG)
#include <assert.h>
#endif

#define TEST_SIZE 200000000

/****** Should be 2^N *****/
#define MAX_CORE_NUM 32

#ifndef CPU_ID
#define CPU_ID cpu_id
#endif

static struct queue_t queues[MAX_CORE_NUM];

struct init_info {
	uint32_t	cpu_id;
	pthread_barrier_t * barrier;
  uint32_t  r;
};

struct init_info info[MAX_CORE_NUM];

#define INIT_ID(p)	(info[p].cpu_id)
#define INIT_BAR(p)	(info[p].barrier)
#define INIT_RES_NUM(p) (info[p].r)
#define INIT_PTR(p)	(&info[p])
#define INIT_INFO struct init_info

inline uint64_t max(uint64_t a, uint64_t b)
{
	return (a > b) ? a : b;
}

#define MAX_RESULTS (1)
struct results {
  uint64_t cons;
  uint64_t prod;
}
results[MAX_RESULTS];

void * consumer(void *arg)
{
	uint32_t 	cpu_id, r;
	ELEMENT_TYPE	value;
	cpu_set_t	cur_mask;
	uint64_t	i;
#if defined(WORKLOAD_DEBUG)
	unsigned long	seed;
#endif
#if defined(FIFO_DEBUG)
	ELEMENT_TYPE	old_value = 0; 
#endif

	INIT_INFO * init = (INIT_INFO *) arg;
	cpu_id = init->cpu_id;
	pthread_barrier_t *barrier = init->barrier;
  r = init->r;

	/* user needs tune this according to their machine configurations. */
	CPU_ZERO(&cur_mask);
	CPU_SET(CPU_ID, &cur_mask);
	//cur_mask = 0x4;
        /*if(cpu_id < 4)
                cur_mask = (0x2<<(2*cpu_id));
        else
                cur_mask = (0x1<<(2*(cpu_id-4)));
	*/

	printf("consumer %d:  ---%d----\n", cpu_id, 2*cpu_id);
	if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
		perror("Error: sched_setaffinity");
		return NULL;
	}

#if defined(WORKLOAD_DEBUG)
	seed = read_tsc();
#endif

	printf("Consumer created...\n");
	pthread_barrier_wait(barrier);

	queues[cpu_id].start_c = read_tsc();

	for (i = 1; i <= TEST_SIZE; i++) {
		while( dequeue(&queues[cpu_id], &value) != 0 );

#if defined(WORKLOAD_DEBUG)
		workload(&seed);
#endif

#if defined(FIFO_DEBUG)
		assert((old_value + 1) == value);
		old_value = value;
#endif
	}
	queues[cpu_id].stop_c = read_tsc();

#if defined(WORKLOAD_DEBUG)
	printf("consumer: %" PRId64 " cycles/op\n", 
		((queues[cpu_id].stop_c - queues[cpu_id].start_c) / (TEST_SIZE + 1)) \
       		- AVG_WORKLOAD);
  results[r].cons = ((queues[cpu_id].stop_c - queues[cpu_id].start_c) / (TEST_SIZE + 1)) - AVG_WORKLOAD;
#else
	printf("consumer: %" PRId64 " cycles/op\n", 
		((queues[cpu_id].stop_c - queues[cpu_id].start_c) / (TEST_SIZE + 1)));
  results[r].cons = ((queues[cpu_id].stop_c - queues[cpu_id].start_c) / (TEST_SIZE + 1));
#endif

	pthread_barrier_wait(barrier);
	return NULL;
}

void producer(void *arg, uint32_t num, uint32_t r)
{
	uint64_t start_p;
	uint64_t stop_p;
	//pthread_barrier_t *barrier = (pthread_barrier_t *)arg;
	uint64_t	i;
	int32_t 	j;
	cpu_set_t	cur_mask;
	INIT_INFO * init = (INIT_INFO *) arg;
	pthread_barrier_t *barrier = init->barrier;

	/* user needs tune this according to their machine configurations. */
	CPU_ZERO(&cur_mask);
	CPU_SET(0, &cur_mask);
	printf("producer %d:  ---%d----\n", 0, 1);
	if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
		perror("Error: sched_setaffinity");
		return ;
	}

	pthread_barrier_wait(barrier);

	start_p = read_tsc();

	for (i = 1; i <= TEST_SIZE + CONS_BATCH_SIZE; i++) {
		for (j=1; j<num; j++) {
			while ( enqueue(&queues[j], (ELEMENT_TYPE)i) != 0);
#if defined(INCURE_DEBUG)
			if(i==(TEST_SIZE >> 1)) {
				printf("Duplicating data to incur bugs\n");
				enqueue(&queues[j], (ELEMENT_TYPE)i);
			}
#endif
		}
	}
	stop_p = read_tsc();

	printf("producer %" PRId64 " cycles/op\n", (stop_p - start_p) / ((TEST_SIZE + 1)*(num -1)));

  results[r].prod = (stop_p - start_p) / ((TEST_SIZE + 1)*(num -1));

	pthread_barrier_wait(barrier);
}

int main(int argc, char *argv[])
{
	pthread_t	consumer_thread;
	pthread_attr_t	consumer_attr;
	pthread_barrier_t barrier;
	int		error     , i, max_th = 2, c;

	if (argc > 1)   {
		max_th = atoi(argv[1]);
	}

	if (max_th < 2) {
		max_th = 2;
		printf("Minimum core number is 2\n");
	}
	if (max_th > MAX_CORE_NUM) {
		max_th = MAX_CORE_NUM;
		printf("Maximum core number is %d\n", max_th);
	}

	srand((unsigned int)read_tsc());

	for (i=0; i<MAX_CORE_NUM; i++) {
		queue_init(&queues[i]);
	}

	error = pthread_barrier_init(&barrier, NULL, max_th);
	if (error != 0) {
		perror("BW");
		return 1;
	}
	error = pthread_attr_init(&consumer_attr);
	if (error != 0) {
		perror("BW");
		return 1;
	}

  for(c = 0; c < MAX_RESULTS; ++c ) {
    /* For N cores, there are N-1 fifos. */
    for (i=1; i<max_th; i++) {
      INIT_ID(i) = i;
      INIT_BAR(i) = &barrier;
      INIT_RES_NUM(i) = c;
      error = pthread_create(&consumer_thread, &consumer_attr, 
            consumer, INIT_PTR(i));
    }
    if (error != 0) {
      perror("BW");
      return 1;
    }

    INIT_ID(0) = 0;
    INIT_BAR(0) = &barrier;
    INIT_RES_NUM(0) = c;
    producer(INIT_PTR(0), max_th, c);
    printf(".\n");
  }
	printf("Done!\n");

  printf("producers: ");
  for(c = 0; c < MAX_RESULTS; ++c ) { printf("%" PRId64 " ", results[c].prod); }
  printf("\nconsumers: ");
  for(c = 0; c < MAX_RESULTS; ++c ) { printf("%" PRId64 " ", results[c].cons); }
  printf("\n");

	return 0;
}
