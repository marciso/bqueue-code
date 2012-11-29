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


#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include "fifo2.hpp"

#if defined(FIFO_DEBUG)
#include <assert.h>
#endif

#define TEST_SIZE 200000000

/****** Should be 2^N *****/
#define MAX_CORE_NUM 8

typedef queue<> queue_t;
typedef uint64_t ELEMENT_TYPE;

static queue_t queues[MAX_CORE_NUM];

struct queue_times_t {
  uint64_t start_c;
  uint64_t stop_c;
}
queues_times[MAX_CORE_NUM];

struct init_info {
	uint32_t	cpu_id;
	pthread_barrier_t * barrier;
};

struct init_info info[MAX_CORE_NUM];

#define INIT_ID(p)	(info[p].cpu_id)
#define INIT_BAR(p)	(info[p].barrier)
#define INIT_PTR(p)	(&info[p])
#define INIT_INFO struct init_info

inline uint64_t max(uint64_t a, uint64_t b)
{
	return (a > b) ? a : b;
}

inline uint64_t read_tsc()
{
        uint64_t        time;
        uint32_t        msw   , lsw;
        __asm__         __volatile__("rdtsc\n\t"
                        "movl %%edx, %0\n\t"
                        "movl %%eax, %1\n\t"
                        :         "=r"         (msw), "=r"(lsw)
                        :   
                        :         "%edx"      , "%eax");
        time = ((uint64_t) msw << 32) | lsw;
        return time;
}

inline void wait_ticks(uint64_t ticks)
{
        uint64_t        current_time;
        uint64_t        time = read_tsc();
        time += ticks;
        do {
                current_time = read_tsc();
        } while (current_time < time);
        printf("wait_ticks\n");
}

void * consumer(void *arg)
{
	uint32_t 	cpu_id;
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

	/* user needs tune this according to their machine configurations. */
	CPU_ZERO(&cur_mask);
	CPU_SET(31, &cur_mask);
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

	queues_times[cpu_id].start_c = read_tsc();

	for (i = 1; i <= TEST_SIZE; i++) {
		while( queues[cpu_id].dequeue(&value) != 0 );

#if defined(WORKLOAD_DEBUG)
		workload(&seed);
#endif

#if defined(FIFO_DEBUG)
		assert((old_value + 1) == value);
		old_value = value;
#endif
	}
	queues_times[cpu_id].stop_c = read_tsc();

  std::ostringstream os;
#if defined(WORKLOAD_DEBUG)
  os << "consumer: "
    << (((queues_times[cpu_id].stop_c - queues_times[cpu_id].start_c) / (TEST_SIZE + 1)) - AVG_WORKLOAD)
    <<  " cycles/op"  << std::endl;
#else
  os << "consumer: "
    << ((queues_times[cpu_id].stop_c - queues_times[cpu_id].start_c) / (TEST_SIZE + 1))
    <<  " cycles/op"  << std::endl;
#endif

  std::cout << os.str();

	pthread_barrier_wait(barrier);
	return NULL;
}

void producer(void *arg, uint32_t num)
{
	uint64_t start_p;
	uint64_t stop_p;
	//pthread_barrier_t *barrier = (pthread_barrier_t *)arg;
	uint64_t	i;
	uint32_t 	j;
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

	for (i = 1; i <= TEST_SIZE + queue_t::consumer_batch_size(); i++) {
		for (j=1; j<num; j++) {
			while ( queues[j].enqueue((ELEMENT_TYPE)i) != 0);
#if defined(INCURE_DEBUG)
			if(i==(TEST_SIZE >> 1)) {
				printf("Duplicating data to incur bugs\n");
				queues[j].enqueue( (ELEMENT_TYPE)i);
			}
#endif
		}
	}
	stop_p = read_tsc();

  std::ostringstream os;
  os << "producer " << ((stop_p - start_p) / ((TEST_SIZE + 1)*(num -1))) << " cycles/op" << std::endl;
  std::cout << os.str();

	pthread_barrier_wait(barrier);
}

int main(int argc, char *argv[])
{
	pthread_t	consumer_thread;
	pthread_attr_t	consumer_attr;
	pthread_barrier_t barrier;
	int		error     , i, max_th = 2;

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

  /*
	for (i=0; i<MAX_CORE_NUM; i++) {
		queue_init(&queues[i]);
	}
  */

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

	/* For N cores, there are N-1 fifos. */
	for (i=1; i<max_th; i++) {
		INIT_ID(i) = i;
		INIT_BAR(i) = &barrier;
		error = pthread_create(&consumer_thread, &consumer_attr, 
					consumer, INIT_PTR(i));
	}
	if (error != 0) {
		perror("BW");
		return 1;
	}

	INIT_ID(0) = 0;
	INIT_BAR(0) = &barrier;
	producer(INIT_PTR(0), max_th);
	printf("Done!\n");

	return 0;
}
