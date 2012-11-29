
 #  B-Queue -- An efficient and practical queueing for fast core-to-core
 #             communication
 #
 #  Copyright (C) 2011 Junchang Wang <junchang.wang@gmail.com>
 #
 #
 #  This program is free software: you can redistribute it and/or modify
 #  it under the terms of the GNU General Public License as published by
 #  the Free Software Foundation, either version 3 of the License, or
 #  (at your option) any later version.
 #
 #  This program is distributed in the hope that it will be useful,
 #  but WITHOUT ANY WARRANTY; without even the implied warranty of
 #  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 #  GNU General Public License for more details.
 #
 #  You should have received a copy of the GNU General Public License
 #  along with this program.  If not, see <http://www.gnu.org/licenses/>.


CC=gcc
#CFLAGS = -g -D_M64_ #-DFIFO_DEBUG #-DWORKLOAD_DEBUG
INCLUDE = ../../include
CFLAGS = -Wall -Werror -g -O2 -D_M64_ -I$(INCLUDE)

#CFLAGS += -DRT_SCHEDULE
#CFLAGS += -DPROD_BATCH
CFLAGS += -DCONS_BATCH
CFLAGS += -DBACKTRACKING
CFLAGS += -DADAPTIVE
#CFLAGS += -DFIFO_DEBUG
#CFLAGS += -DWORKLOAD_DEBUG
#CFLAGS += -DINCURE_DEBUG

CXXFLAGS = $(CFLAGS)

ORG = fifo.o main.o workload.o

fifo: $(ORG) $(LIB) test_cycle test2 test3
	gcc $(ORG) $(LIB) -o $@ -lpthread

$(ORG): fifo.h Makefile

fifo2.cpp: fifo2.hpp
test3.cpp: fifo2.o fifo2.hpp

test3: $(LIB) test3.o fifo2.o
	g++ fifo2.o $< -o $@

test2: $(LIB) test2.o
	gcc  fifo.o $< -o $@ -lm

test_cycle: test_cycle.o workload.o
	gcc $(LIB) workload.o $< -o $@

test_cycle.o: fifo.h Makefile

clean:
	rm -f $(ORG) fifo test_cycle test_cycle.o cscope* test2 test2.o fifo2.o test3 test3.o
