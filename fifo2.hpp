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
//#include <stdio.h>
//#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <stdint.h>

// The queue claims internal buffer in batches (if CONS_BATCH/PROD_BATCH == false,
// then the batch size is 1).
//
// BACKTRACKING is used by consumers if CONS_BATCH == true; when trying to get elements in region
// smaller than CONS_BATCH_SIZE.
//
// ADAPTIVE is performance improvement for customers, it adjusts batch_history, so BACKTRACKING
// doesn't have to start from the same CONS_BATCH value
// (ADAPTIVE is used only in BACKTRACKING and it equals CONS_BATCH_SIZE at the beginning)


template<size_t QUEUE_SIZE = (1024 * 8), typename ELEMENT_TYPE = uint64_t, size_t CONGESTION_PENALTY_CYCLES = 1000,
  bool CONS_BATCH = true, bool PROD_BATCH = false, bool BACKTRACKING = true, bool ADAPTIVE = true>
class queue
{
public:
  enum ReturnCode { SUCCESS=0, BUFFER_FULL=1, BUFFER_EMPTY=2 };

  static size_t queue_size() { return QUEUE_SIZE; }
  static size_t congesion_penalty() { return CONGESTION_PENALTY_CYCLES; }
  static bool is_consumer_batching() { return CONS_BATCH; }
  static bool is_producer_batching() { return PROD_BATCH; }
  static bool is_consumer_backtraking() { return BACKTRACKING; }
  static bool is_consumer_backtraking_adaptive() { return ADAPTIVE; }
  static size_t consumer_batch_size() { return CONS_BATCH ? CONS_BATCH_SIZE : 0U; }
  static size_t producer_batch_size() { return PROD_BATCH ? PROD_BATCH_SIZE : 0U; }

  queue() : head(0U), batch_head (0U)
    , tail(0U), batch_tail(0U), batch_history(CONS_BATCH_SIZE)
    //, data() // calls default constructor for array elements
  {
    for(size_t i = 0U; i < QUEUE_SIZE; ++i) {
      this->data[i] = ELEMENT_ZERO;
    }
  }

  enum ReturnCode enqueue(ELEMENT_TYPE value)
  {
    if ( PROD_BATCH ) {

      if( this->head == this->batch_head ) {
        // try to allocate another batch
        uint32_t tmp_head = this->head + PROD_BATCH_SIZE;
        if ( tmp_head >= QUEUE_SIZE ) { tmp_head = 0; }

        if ( ELEMENT_ZERO != this->data[tmp_head] ) {
          wait_ticks<true, true>(CONGESTION_PENALTY_CYCLES);
          // fail if the whole batch cannot be allocated
          return BUFFER_FULL;
        }

        this->batch_head = tmp_head;
      }

    }
    else {

      if ( ELEMENT_ZERO != this->data[this->head] ) {
        // fail because this->head points at occupied element
        return BUFFER_FULL;
      }

    }

    this->data[this->head] = value;
    this->head ++;
    if ( this->head >= QUEUE_SIZE ) { this->head = 0; }

    return SUCCESS;
  }

  enum ReturnCode dequeue(ELEMENT_TYPE *value)
  {
    if ( CONS_BATCH ) {

      if( this->tail == this->batch_tail ) {
        bool const b = this->backtracking< BACKTRACKING, ADAPTIVE >();
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
    else {

      if ( ELEMENT_ZERO == this->data[this->tail] )
        return BUFFER_EMPTY;

      *value = this->data[this->tail];
      this->data[this->tail] = ELEMENT_ZERO;
      this->tail ++;
      if ( this->tail >= QUEUE_SIZE )
        this->tail = 0;

      return SUCCESS;

    }
  }

private:
  enum { CONS_BATCH_SIZE   = (QUEUE_SIZE/16) , BATCH_INCREAMENT  = (QUEUE_SIZE/32) }; // used iff CONS_BATCH
  enum { PROD_BATCH_SIZE   = (QUEUE_SIZE/16) }; // used iff PROD_BATCH


  /* Mostly accessed by producer. */
  volatile	uint32_t	head;
  volatile	uint32_t	batch_head; // used iff PROD_BATCH

  /* Mostly accessed by consumer. */
  volatile	uint32_t	tail __attribute__ ((aligned(64)));
  volatile	uint32_t	batch_tail; // used iff CONS_BATCH
  size_t batch_history; // used iff CONS_BATCH

  /* Accessed by both producer and comsumer */
  ELEMENT_TYPE	data[QUEUE_SIZE] __attribute__ ((aligned(64)));

  static const ELEMENT_TYPE ELEMENT_ZERO = 0x0UL;

  template<bool BACKTRACKING_, bool ADAPTIVE_> bool backtracking()
  {
    uint32_t tmp_tail;
    tmp_tail = this->tail + CONS_BATCH_SIZE;
    if ( tmp_tail >= QUEUE_SIZE ) {
      tmp_tail = 0;

      if ( ADAPTIVE_ ) {
        if (this->batch_history < CONS_BATCH_SIZE) {
          this->batch_history =
            (CONS_BATCH_SIZE < (this->batch_history + BATCH_INCREAMENT)) ?
            CONS_BATCH_SIZE : (this->batch_history + BATCH_INCREAMENT);
        }
      }

    }

    if ( BACKTRACKING_ ) {

      size_t batch_size = this->batch_history;
      while ( ELEMENT_ZERO == this->data[tmp_tail] ) {

        wait_ticks<true, true>(CONGESTION_PENALTY_CYCLES); // give a chance for producer to extend the buffer

        batch_size = batch_size >> 1;
        if( batch_size >= 0 ) {
          tmp_tail = this->tail + batch_size;
          if (tmp_tail >= QUEUE_SIZE)
            tmp_tail = 0;
        }
        else {
          //std::cout << "backtrack: " << BACKTRACKING_ << ", adaptive: " << ADAPTIVE_ << ", returns false (1)" << std::endl;
          return false;
        }
      }

      if ( ADAPTIVE_ ) {
        this->batch_history = batch_size;
      }

    }
    else {
      if ( ELEMENT_ZERO == this->data[tmp_tail] ) {
        wait_ticks<true, true>(CONGESTION_PENALTY_CYCLES);
        //std::cout << "backtrack: " << BACKTRACKING_ << ", adaptive: " << ADAPTIVE_ << ", returns false (2)" << std::endl;
        return false;
      }
    }

    if ( tmp_tail == this->tail ) {
      tmp_tail = (tmp_tail + 1) >= QUEUE_SIZE ?
        0 : tmp_tail + 1;
    }
    this->batch_tail = tmp_tail;

    //std::cout << "backtrack: " << BACKTRACKING_ << ", adaptive: " << ADAPTIVE_ << ", returns true" << std::endl;
    return true;
  }

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

  static inline uint64_t read_tsc()
  {
    uint32_t msw, lsw;
    asm volatile("rdtsc; movl %%edx, %0; movl %%eax, %1" : "=r" (msw), "=r"(lsw) :: "%edx", "%eax");
    uint64_t const time = ((uint64_t) msw << 32) | lsw;
    return time;
  }

  static inline void cpu_relax() { asm volatile("rep; nop" ::: "memory"); }

# if defined(__i386__)
  static inline void rmb() { asm volatile("lock; addl $0,0(%%esp)" ::: "memory"); }
# endif

# if defined(__x86_64__)
  static inline void rmb() { asm volatile("lfence" ::: "memory"); }
# endif

#else

  // On Windows could use QueryPerformanceCounter and QueryPerformanceFrequency.
  static inline uint64_t read_tsc() { return CONGESTION_PENALTY_CYCLES + 1; }

  // For Windows one could use:
  //   _mm_pause(); // for MSVC/IA-32
  //   __yield(); // for MSVC/IA-64
  //Ref: http://www.1024cores.net/home/lock-free-algorithms/tricks/spinning
  static inline void cpu_relax() { }
  static inline void rmb() { }
#endif

  template<bool MEMORY_BARRIER, bool NOP>
    static inline void wait_ticks(uint64_t ticks)
  {
    uint64_t const time = read_tsc() + ticks;
    uint64_t current_time = read_tsc();
    while(current_time < time) {
      if ( NOP ) { cpu_relax(); }
      if ( MEMORY_BARRIER ) { rmb(); }
      // OPEN: add memory barrier or NOP instructions here
      current_time = read_tsc();
    }
  }

} __attribute__ ((aligned(64)));


#endif

