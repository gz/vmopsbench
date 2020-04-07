/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */

#ifndef __VMOPS_BENCHMARKS_H_
#define __VMOPS_BENCHMARKS_H_ 1

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../platform/platform.h"
#include "../logging.h"

struct vmops_bench_cfg {
    const char *benchmark;
    uint32_t *coreslist;
    uint32_t  corelist_size;
    size_t memsize;
    size_t nmaps;
    uint32_t time_ms;
    bool nounmap;
    bool shared;
    bool isolated;
    bool map4k;
    bool maphuge;
    bool numainterleave;
};


struct vmops_bench_run_arg
{
    struct vmops_bench_cfg *cfg;
    plat_memobj_t memobj;
    plat_thread_t thread;
    uint32_t tid;
    uint32_t coreid;
    plat_barrier_t barrier;
    size_t count;
    double duration;
};


/**
 * @brief starts the maponly or mapunmap benchmark
 *
 * @param cfg   the benchmark configuration
 * @param opts  the options for the benchmark
 *
 * @returns 0 success, -1 error
 */
int vmpos_bench_run_mapunmap(struct vmops_bench_cfg *cfg,
                             const char *opts);

/**
 * @brief starts the page protection benchmark
 *
 * @param cfg   the benchmark configuration
 * @param opts  the options for the benchmark
 *
 * @returns 0 success, -1 error
 */
int vmops_bench_run_protect(struct vmops_bench_cfg *cfg,
                              const char *opts);


#endif /* __VMOPS_BENCHMARKS_H_ */