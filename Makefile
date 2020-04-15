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

bin/vmops : $(DEPS_ALL) bin
	$(CC) $(CFLAGS) -o $@ src/main.c $(BENCHMARK_FILES) src/platform/$(PLATFORM).c $(LIBS)

bin/vmopstrace : $(DEPS_ALL) bin
	$(CC) $(CFLAGS) -g -fno-omit-frame-pointer -o $@ src/main.c $(BENCHMARK_FILES) src/platform/$(PLATFORM).c $(LIBS)


contrib/flamegraph :
	git clone https://github.com/brendangregg/FlameGraph.git $@


profileprep :
	echo 0 | sudo tee  /proc/sys/kernel/kptr_restrict
	echo "-1" | sudo tee /proc/sys/kernel/perf_event_paranoid > /dev/null
	sudo sysctl -w vm.max_map_count=2000000000

perfdata:
	mkdir -p perfdata

#profile-clean:
	#rm -rf perfdata

perfdata/maponly-isolated.perf : perfdata bin/vmopstrace
	make profileprep
	perf record -o $@ --delay=1 -g ./bin/vmopstrace -b maponly-isolated -t 5000

perfdata/maponly-default.perf: perfdata bin/vmopstrace
	make profileprep
	perf record -o $@ --delay=1 -g ./bin/vmopstrace -b maponly -t 5000

perfdata/maponly-isolated-4.perf: perfdata bin/vmopstrace
	make profileprep
	perf record -o $@ --delay=1 -g ./bin/vmopstrace -b maponly-isolated -p 4 -t 5000

perfdata/maponly-default-4.perf: perfdata bin/vmopstrace
	make profileprep
	perf record -o $@ --delay=1 -g ./bin/vmopstrace -b maponly -p 4-t 5000


perfdata/%.out : perfdata/%.perf
	perf script -i $< > $@
.PRECIOUS: perfdata/%.out

perfdata/%.folded : perfdata/%.out contrib/flamegraph
	./contrib/flamegraph/stackcollapse-perf.pl $< > $@
.PRECIOUS: perfdata/%.folded

perfdata/%.svg : perfdata/%.folded contrib/flamegraph
	./contrib/flamegraph/flamegraph.pl --title "$@" --width 3500   $< > $@

profile-maponly-default : perfdata/maponly-default.svg Makefile
	xdg-open perfdata/maponly-default.svg

profile-maponly-isolated : perfdata/maponly-isolated.svg Makefile
	xdg-open perfdata/maponly-isolated.svg

profile-maponly-default-4 : perfdata/maponly-default-4.svg Makefile
	xdg-open perfdata/maponly-default-4.svg

profile-maponly-isolated-4 : perfdata/maponly-isolated-4.svg Makefile
	xdg-open perfdata/maponly-isolated-4.svg

clean:
	rm -rf bin
