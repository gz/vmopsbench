/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */

#ifndef __VMOPS_BENCH_UTILS_H_
#define __VMOPS_BENCH_UTILS_H_ 1

#include "benchmarks.h"


/*
 * ================================================================================================
 * Option Parsing and Printing
 * ================================================================================================
 */


/**
 * @brief parses the benchmark options
 *
 * @param opts  the options string
 * @param cfg   the benchmark config struct
 *
 * @returns 0 on success,  -1 on failure
 */
int vmops_utils_parse_options(const char *opts, struct vmops_bench_cfg *cfg);


/**
 * @brief prints the benchmark options
 *
 * @param cfg   the benchmark config
 *
 * @returns formatted string with the options
 */
char *vmops_utils_print_options(struct vmops_bench_cfg *cfg);


/*
 * ================================================================================================
 * Result Printing
 * ================================================================================================
 */


/**
 * @brief prints the results of the benchmark as CSV
 *
 * @param args          the benchmark arguments
 * @param total_ops     the total number of operations executed
 */
void vmops_utils_print_csv(struct vmops_bench_run_arg *args);


/*
 * ================================================================================================
 * Benchmark Argument Preparation
 * ================================================================================================
 */


/**
 * @brief prepares the arguments for teh benchmark threads
 *
 * @param cfg       the benchmark configs
 * @param retargs   returns the thread arguments array
 *
 * @returns 0 on success, -1 on failure
 */
int vmops_utils_prepare_args(struct vmops_bench_cfg *cfg, struct vmops_bench_run_arg **retargs);


/**
 * @brief cleans up the previously prepared arguments
 *
 * @param args      the benchmark thread arguments
 *
 * @returns 0 on success, -1 on failure
 */
int vmops_utils_cleanup_args(struct vmops_bench_run_arg *args);


/*
 * ================================================================================================
 * Benchmark Running
 * ================================================================================================
 */


/**
 * @brief generic run function for the benchmark threads
 *
 * @param nthreads  number of arguments
 * @param args      the arguments for the threads
 * @param runfn     the function to be run
 *
 * @returns 0 on success, -1 on failure
 */
int vmops_utils_run_benchmark(uint32_t nthreads, struct vmops_bench_run_arg *args,
                              plat_thread_fn_t runfn);


/*
 * ================================================================================================
 * Address Mapping Offset
 * ================================================================================================
 */

#define ADDRESS_OFFSET (512UL << 30)

static inline void *utils_vmops_get_map_address(uint32_t tid)
{
    return (void *)(ADDRESS_OFFSET * (tid + 1));
}


/*
 * ================================================================================================
 * Statistics
 * ================================================================================================
 */


static inline void vmops_utils_add_stats(struct vmops_stats *stats, uint32_t tid, uint64_t ops,
                                         plat_time_t t_elapsed, plat_time_t val)
{
    if (stats->sampling_next > t_elapsed) {
        return;
    }

    if (stats->idx == stats->idx_max) {
        return;
    }

    stats->values[stats->idx] = (struct statval){tid, t_elapsed, ops, val};
    stats->idx++;
    stats->sampling_next = t_elapsed + stats->sampling_delta;
}


#endif /* __VMOPS_BENCH_UTILS_H_ */