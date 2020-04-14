#
# Virtual Memory Operations Benchmark
#
# Copyright 2020 Reto Achermann
# SPDX-License-Identifier: GPL-3.0
#

PLATFORM=linux

# Compiler and flags to use
CC=gcc
COMMON_CFLAGS=-O3 -Wall -Wextra -std=c17

BENCHMARK_FILES=$(wildcard src/benchmarks/*.c)

# dependencies
DEPS_SRC=$(wildcard src/*.c) $(wildcard src/platform/*.c) $(wildcard src/benchmarks/*.c)
DEPS_INC=$(wildcard src/*.h) $(wildcard src/platform/*.h) $(wildcard src/benchmarks/*.h)
DEPS_ALL=bin $(DEPS_SRC) $(DEPS_INC) Makefile


ifeq ($(PLATFORM),linux)
  PLAT_LIBS=-lnuma -lrt -lpthread
else
  PLAT_LIBS=
endif


CFLAGS=$(COMMON_CFLAGS) $(PLAT_CFLAGS)
LIBS=$(COMMON_LIBS) $(PLAT_LIBS)


# build targets
all: bin/vmops bin/vmopstrace

bin:
	mkdir -p bin

bin/vmops: $(DEPS_ALL)
	$(CC) $(CFLAGS) -o bin/vmops src/main.c $(BENCHMARK_FILES) src/platform/$(PLATFORM).c $(LIBS)

bin/vmopstrace : $(DEPS_ALL)
	$(CC) $(CFLAGS) -g -fno-omit-frame-pointer -o bin/vmopstrace src/main.c $(BENCHMARK_FILES) src/platform/$(PLATFORM).c $(LIBS)


profileone : bin/vmopstrace
	echo 0 | sudo tee  /proc/sys/kernel/kptr_restrict
	echo "-1" | sudo tee /proc/sys/kernel/perf_event_paranoid > /dev/null
	sudo sysctl -w vm.max_map_count=2000000000
	perf record --delay=1 -g ./bin/vmopstrace -b maponly -t 5000

profilefour : bin/vmopstrace
	echo 0 | sudo tee  /proc/sys/kernel/kptr_restrict
	echo "-1" | sudo tee /proc/sys/kernel/perf_event_paranoid > /dev/null
	sudo sysctl -w vm.max_map_count=2000000000
	perf record --delay=1 -g ./bin/vmopstrace -p 4 -b maponly -t 5000

report :
	perf report --stdio -g none  --sort  overhead_sys
	perf report --stdio -g none --sort overhead
	perf report --stdio --call-graph=flat
	perf report --stdio -g none

clean:
	rm -rf bin
