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


#include "fifo2.hpp"
#include <sched.h>

#include <iostream>

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
        //std::cout << "wait_ticks" << std::endl;
}

static ELEMENT_TYPE ELEMENT_ZERO = 0x0UL;

/*************************************************/
/********** Queue Functions **********************/
/*************************************************/

void queue::init()
{
  this->head = this->tail = 0;
#if defined(CONS_BATCH) || defined(PROD_BATCH)
  this->batch_head = this->batch_tail = 0;
#endif

	memset(this->data, 0, sizeof(this->data));
#if defined(CONS_BATCH)
	this->batch_history = CONS_BATCH_SIZE;
#endif
}

#if defined(PROD_BATCH)
enum queue::ReturnCode queue::enqueue(ELEMENT_TYPE value)
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
enum queue::ReturnCode queue::enqueue(ELEMENT_TYPE value)
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

#if defined(CONS_BATCH)

template<bool BACKTRACKING_, bool ADAPTIVE_> bool queue::backtracking()
{
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

enum queue::ReturnCode queue::dequeue(ELEMENT_TYPE * value)
{
	if( this->tail == this->batch_tail ) {
    bool const b = this->backtracking<
#ifdef BACKTRACKING
      true
#else
      false
#endif
      ,
#ifdef ADAPTIVE
      true
#else
      false
#endif
      >();

		if ( !b )
			return BUFFER_EMPTY;
	}
	*value = this->data[this->tail];
	this->data[this->tail] = ELEMENT_ZERO;
	this->tail ++;
	if ( this->tail >= QUEUE_SIZE )
		this->tail = 0;

	return SUCCESS;
}

#else

enum queue::ReturnCode queue::dequeue(ELEMENT_TYPE * value)
{
	if ( !this->data[this->tail] )
		return BUFFER_EMPTY;
	*value = this->data[this->tail];
	this->data[this->tail] = ELEMENT_ZERO;
	this->tail ++;
	if ( this->tail >= QUEUE_SIZE )
		this->tail = 0;

	return SUCCESS;
}

#endif
