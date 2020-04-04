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

#define PAGE_SIZE 4096

struct vmops_bench_cfg {
    const char *benchmark;
    uint32_t *coreslist;
    uint32_t  corelist_size;
    size_t memsize;
    size_t nmaps;
    uint32_t time_ms;
    bool nounmap;
};

/**
 * @brief runs the vmops benchmark in the isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - each thread has an independent memory object to be mapped
 *  - each thread maps it in a distinct slot in the root page table
 */
void vmops_bench_run_isolated(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the concurrent protect configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - each thread has an independent memory object to be mapped
 *  - virtual region mapped with default OS policy
 */
void vmops_bench_run_independent(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - the memory object is shared
 *  - each thread maps it in a distinct slot in the root page table
 */
void vmops_bench_run_shared_isolated(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - the memory object is shared
 *  - each thread maps it in a distinct slot in the root page table
 */
void vmops_bench_run_4k_shared_isolated(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - the memory object is shared
 *  - virtual memory address is allocated using the default OS policy
 */
void vmops_bench_run_shared_independent(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - the memory object is shared
 *  - each thread maps it in a distinct slot in the root page table
 */
void vmops_bench_run_4k_shared_independent(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - the memory object is shared
 *  - each thread maps it in a distinct slot in the root page table
 */
void vmops_bench_run_4k_isolated(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - each thread has its own memory object
 *  - virtual memory address is allocated using the default OS policy
 *  - memory object is mapped using multiple 4k mappings
 */
void vmops_bench_run_4k_independent(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the concurrent protect configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - there is a single shared memory region
 *  - calling randomly protect() on pages.
 */
void vmops_bench_run_protect_shared(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the concurrent protect configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - there is a shared memory region per thread
 *  - calling randomly protect() on pages.
 */
void vmops_bench_run_protect_independent(struct vmops_bench_cfg *cfg);


/**
 * @brief runs the vmops benchmark in the concurrent protect configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - each thread has its own shared memory regoin
 *  - it is mapped using separate 4k mappings
 *  - protect / unprotect one fo these.
 */
void vmops_bench_run_protect_4k_independent(struct vmops_bench_cfg *cfg);


#endif /* __VMOPS_BENCHMARKS_H_ */