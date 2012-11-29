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


#ifndef _FIFO2_B_QUQUQ_H_
#define _FIFO2_B_QUQUQ_H_

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <stdint.h>


#define ELEMENT_TYPE uint64_t

struct queue{

  enum {  QUEUE_SIZE=(1024 * 8), 
          BATCH_SIZE=(QUEUE_SIZE/16),
          CONS_BATCH_SIZE=BATCH_SIZE,
          PROD_BATCH_SIZE=BATCH_SIZE,
          BATCH_INCREAMENT=(BATCH_SIZE/2),
          CONGESTION_PENALTY=(1000) /* cycles */ };

  enum ReturnCode { SUCCESS=0, BUFFER_FULL=-1, BUFFER_EMPTY=-2 };

	/* Mostly accessed by producer. */
	volatile	uint32_t	head;
#if defined(CONS_BATCH) || defined(PROD_BATCH)
	volatile	uint32_t	batch_head;
#endif

	/* Mostly accessed by consumer. */
	volatile	uint32_t	tail __attribute__ ((aligned(64)));
#if defined(CONS_BATCH) || defined(PROD_BATCH)
	volatile	uint32_t	batch_tail;
	unsigned long	batch_history;
#endif

	/* accessed by both producer and comsumer */
	ELEMENT_TYPE	data[QUEUE_SIZE] __attribute__ ((aligned(64)));

  void init();
  enum ReturnCode enqueue(ELEMENT_TYPE value);
  enum ReturnCode dequeue(ELEMENT_TYPE *value);

private:
#ifdef CONS_BATCH
  template<bool BACKTRACKING_, bool ADAPTIVE_> bool backtracking();
#endif
} __attribute__ ((aligned(64)));

#endif

