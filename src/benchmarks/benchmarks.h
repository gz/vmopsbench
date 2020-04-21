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

///< the default rate of sampling
#define DEFAULT_SAMPLING_RATE_MS 10

struct vmops_bench_cfg {
    const char *benchmark;
    uint32_t *coreslist;
    uint32_t  corelist_size;
    size_t memsize;
    size_t nmaps;
    uint32_t time_ms;
    uint32_t stats;
    uint32_t rate;
    bool nounmap;
    bool shared;
    bool isolated;
    bool map4k;
    bool maphuge;
    bool numainterleave;
};

struct statval
{
    uint32_t tid;
    plat_time_t t_elapsed;
    uint64_t counter;
    plat_time_t val;
};

struct vmops_stats
{
    struct statval *values;
    size_t idx;
    size_t idx_max;
    plat_time_t sampling_delta;
    plat_time_t sampling_next;
};

#define VMOPS_STATS_MAX 10000000

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
    struct vmops_stats stats;
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