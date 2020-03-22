/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */


#include <stdint.h>
#include <stdlib.h>

#include "benchmarks.h"
#include "utils.h"

/**
 * @brief runs the vmops benchmark in the concurrent protect configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - there is a single shared memory region
 *  - calling randomly protect() on pages.
 */
void vmops_bench_run_protect_shared(struct vmops_bench_cfg *cfg)
{
    (void)(cfg);
}