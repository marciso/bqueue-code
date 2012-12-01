
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

#CCNAME=icc
#CC=/opt/intel/bin/icc -xHOST -ipo
#CXX=/opt/intel/bin/icpc -xHOST -ipo
#
#CCNAME=gcc
#CC=/opt/gcc-4.7.2/bin/gcc
#CXX=/opt/gcc-4.7.2/bin/g++
#
CCNAME=gcc
CC=gcc
CXX=g++
#
#CCNAME=clang
#CC=clang
#CXX=clang++

CCVERSION = $(shell $(CC) -dumpversion)

N=-$(CCNAME)-$(CCVERSION)

#CFLAGS = -g -D_M64_ #-DFIFO_DEBUG #-DWORKLOAD_DEBUG
#INCLUDE = ../../include
#CFLAGS += -Wall -Werror -g -O3 -D_M64_ -I$(INCLUDE)
CFLAGS += -Wall -g -O3 -D_M64_ #-I$(INCLUDE)


CXXFLAGS = $(CFLAGS)

#ORG = fifo.o main.o workload.o

all: fifo$N test2$N test3$N test4$N

fifo$N: fifo.o main.o
	$(CC) main.o fifo.o -o $@ -lpthread

cpu_tests:
	for i in 1 3 7 15 31 ; do make clean ; CFLAGS=-DCPU_ID=$$i make; mv fifo$N fifo$N-cpuid$$i ; mv test4$N test4$N-cpuid$$i ; done

$(ORG): fifo.h Makefile

test3.cpp: fifo2.hpp
test4.cpp: fifo2.hpp

test4$N: test4.o
	$(CXX) $< -o $@  -lpthread

test3$N: test3.o
	$(CXX) $< -o $@

test2$N: test2.o
	$(CC)  fifo.o $< -o $@ -lm

test_cycle$N: test_cycle.o workload.o
	$(CC) $< workload.o  -o $@

test_cycle.o: fifo.h Makefile

clean:
	rm -f $(ORG) fifo$N test_cycle$N test_cycle.o workload.o cscope* test2$N test2.o fifo.o main.o test3$N test3.o test4$N test4.o

cleanall: clean
	rm -f fifo-[ig]cc-* test2-[ig]cc-* test3-[ig]cc-* test4-[ig]cc-* test_cycle-[ig]cc-*
	rm -f fifo-*-cpuid* test4-*-cpuid*

