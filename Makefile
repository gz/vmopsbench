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
DEPS_ALL=bin $(DEPS_SRC) $(DEPS_INC)


ifeq ($(PLATFORM),linux)
  PLAT_LIBS=-lnuma -lrt -lpthread
else
  PLAT_LIBS=
endif


CFLAGS=$(COMMON_CFLAGS) $(PLAT_CFLAGS)
LIBS=$(COMMON_LIBS) $(PLAT_LIBS)


# build targets
all: bin/vmops

bin:
	mkdir -p bin

bin/vmops: $(DEPS_ALL)
	$(CC) $(CFLAGS) -o bin/vmops src/main.c $(BENCHMARK_FILES) src/platform/$(PLATFORM).c $(LIBS)

clean:
	rm -rf bin
