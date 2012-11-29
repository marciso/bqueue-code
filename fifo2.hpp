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

//#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <stdint.h>


template<typename ELEMENT_TYPE = uint64_t>
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

  void init()
  {
    this->head = this->tail = 0;
#if defined(CONS_BATCH) || defined(PROD_BATCH)
    this->batch_head = this->batch_tail = 0;
#endif

    static ELEMENT_TYPE ELEMENT_ZERO = 0x0UL;

    for(size_t i = 0U; i < QUEUE_SIZE; ++i ) {
      this->data[i] = ELEMENT_ZERO;
    }
#if defined(CONS_BATCH)
    this->batch_history = CONS_BATCH_SIZE;
#endif
  }

  enum ReturnCode enqueue(ELEMENT_TYPE value);
  enum ReturnCode dequeue(ELEMENT_TYPE *value);

private:

#if defined(CONS_BATCH)
  template<bool BACKTRACKING_, bool ADAPTIVE_> bool backtracking()
  {
    std::cout << "backtrack: " << BACKTRACKING_ << ", adaptive: " << ADAPTIVE_ << std::endl;
    uint32_t tmp_tail;
    tmp_tail = this->tail + CONS_BATCH_SIZE;
    if ( tmp_tail >= QUEUE_SIZE ) {
      tmp_tail = 0;
      if ( ADAPTIVE_ ) {
        if (this->batch_history < CONS_BATCH_SIZE) {
          this->batch_history = 
            (CONS_BATCH_SIZE < (this->batch_history + BATCH_INCREAMENT))? 
            CONS_BATCH_SIZE : (this->batch_history + BATCH_INCREAMENT);
        }
      }
    }

    if ( BACKTRACKING_ ) {

      unsigned long batch_size = this->batch_history;
      while (!(this->data[tmp_tail])) {

        //std::cout << "tmp_tail=" << tmp_tail << std::endl;
        wait_ticks(CONGESTION_PENALTY);

        batch_size = batch_size >> 1;
        if( batch_size >= 0 ) {
          tmp_tail = this->tail + batch_size;
          if (tmp_tail >= QUEUE_SIZE)
            tmp_tail = 0;
        }
        else
          return false;
      }

      if ( ADAPTIVE_ ) {
        this->batch_history = batch_size;
      }

    }
    else {
      if ( !this->data[tmp_tail] ) {
        //std::cout << "2. tmp_tail=" << tmp_tail << std::endl;
        wait_ticks(CONGESTION_PENALTY); 
        return false;
      }
    }

    if ( tmp_tail == this->tail ) {
      tmp_tail = (tmp_tail + 1) >= QUEUE_SIZE ?
        0 : tmp_tail + 1;
    }
    this->batch_tail = tmp_tail;

    return true;
  }
#endif
  static uint64_t read_tsc()
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

  static void wait_ticks(uint64_t ticks)
  {
          uint64_t        current_time;
          uint64_t        time = read_tsc();
          time += ticks;
          do {
                  current_time = read_tsc();
          } while (current_time < time);
          //std::cout << "wait_ticks" << std::endl;
  }

} __attribute__ ((aligned(64)));



#if defined(CONS_BATCH)

template<typename ELEMENT_TYPE>
enum queue<ELEMENT_TYPE>::ReturnCode queue<ELEMENT_TYPE>::dequeue(ELEMENT_TYPE * value)
{
	if( this->tail == this->batch_tail ) {
    bool const b = this->backtracking<
#if defined(BACKTRACKING)
      true
#else
      false
#endif
      ,
#if defined(ADAPTIVE)
      true
#else
      false
#endif
      >();

		if ( !b )
			return BUFFER_EMPTY;
	}

  static ELEMENT_TYPE ELEMENT_ZERO = 0x0UL;

	*value = this->data[this->tail];
	this->data[this->tail] = ELEMENT_ZERO;
	this->tail ++;
	if ( this->tail >= QUEUE_SIZE )
		this->tail = 0;

	return SUCCESS;
}

#else

template<typename ELEMENT_TYPE>
enum queue<ELEMENT_TYPE>::ReturnCode queue<ELEMENT_TYPE>::dequeue(ELEMENT_TYPE * value)
{
	if ( !this->data[this->tail] )
		return BUFFER_EMPTY;

  static ELEMENT_TYPE ELEMENT_ZERO = 0x0UL;

	*value = this->data[this->tail];
	this->data[this->tail] = ELEMENT_ZERO;
	this->tail ++;
	if ( this->tail >= QUEUE_SIZE )
		this->tail = 0;

	return SUCCESS;
}

#endif


#if defined(PROD_BATCH)
template<typename ELEMENT_TYPE>
enum queue<ELEMENT_TYPE>::ReturnCode queue<ELEMENT_TYPE>::enqueue(ELEMENT_TYPE value)
{
  std::cout << "enqueue value=" << value << std::endl;
	uint32_t tmp_head;
	if( this->head == this->batch_head ) {
		tmp_head = this->head + PROD_BATCH_SIZE;
		if ( tmp_head >= QUEUE_SIZE )
			tmp_head = 0;

		if ( this->data[tmp_head] ) {
      //std::cout << "tmp_head=" << tmp_head << std::endl;
      wait_ticks(CONGESTION_PENALTY);
			return BUFFER_FULL;
		}

		this->batch_head = tmp_head;
	}
	this->data[this->head] = value;
	this->head ++;
	if ( this->head >= QUEUE_SIZE ) {
		this->head = 0;
	}

	return SUCCESS;
}

#else

template<typename ELEMENT_TYPE>
enum queue<ELEMENT_TYPE>::ReturnCode queue<ELEMENT_TYPE>::enqueue(ELEMENT_TYPE value)
{
  //std::cout << "enqueue value=" << value << std::endl;
	if ( this->data[this->head] )
		return BUFFER_FULL;
	this->data[this->head] = value;
	this->head ++;
	if ( this->head >= QUEUE_SIZE ) {
		this->head = 0;
	}

	return SUCCESS;
}
#endif

#endif

