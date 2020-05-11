
/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "benchmarks.h"
#include "utils.h"

#define DEFAULT_NOPS (1000000)


static void *bench_run_fn(struct vmops_bench_run_arg *args)
{
    plat_error_t err;

    struct vmops_bench_cfg *cfg = args->cfg;

    size_t nops = cfg->nops;
    if (cfg->nops == 0) {
        nops = DEFAULT_NOPS;
    }

    size_t total_map_size = (nops * PLAT_ARCH_BASE_PAGE_SIZE);

    size_t counter = 0;
    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    void *addr = utils_vmops_get_map_address(args->tid);
    if (!cfg->isolated) {
        addr = NULL;
    }

    printf("%s %d\n", __FUNCTION__, __LINE__);

    for (size_t i = 0; i < total_map_size; i += cfg->memsize) {
        if (cfg->isolated) {
            void *curaddr = ((char *)addr + i);
            err = plat_vm_map_fixed(curaddr, cfg->memsize, args->memobj, 0, 0);
        } else {
            void *curaddr;
            err = plat_vm_map(&curaddr, cfg->memsize, args->memobj, 0, 0);
            if (addr == NULL) {
                addr = curaddr;
            }
        }

        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d failed to map memory %zu / %zu kB. exiting.\n", args->tid, i >> 10,
                    total_map_size >> 10);
            return NULL;
        }

        err = plat_vm_protect(addr, cfg->memsize, PLAT_PERM_READ_ONLY);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d failed to protect memory. exiting.\n", args->tid);
        }
    }


    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);

    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    while (t_current < t_end && counter < nops) {
        err = plat_vm_protect(addr, PLAT_ARCH_BASE_PAGE_SIZE, PLAT_PERM_READ_WRITE);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
            goto err_out;
        }
        t_current = plat_get_time();
        counter++;
        void *addr = ((char *)addr + PLAT_ARCH_BASE_PAGE_SIZE);
    }
    t_end = plat_get_time();

    plat_thread_barrier(args->barrier);

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

    return NULL;

err_out:
    plat_vm_unmap(addr, cfg->memsize);
    return NULL;
}


/**
 * @brief starts the page protection benchmark
 *
 * @param cfg   the benchmark configuration
 * @param opts  the options for the benchmark
 *
 * @returns 0 success, -1 error
 */
int vmops_bench_run_protect_elevate(struct vmops_bench_cfg *cfg, const char *opts)
{
    if (vmops_utils_parse_options(opts, cfg)) {
        LOG_ERR("failed to parse the options\n");
        return -1;
    }

    LOG_INFO("Preparing benchmark. 'protect-elevate' with options '%s'\n",
             vmops_utils_print_options(cfg));

    struct vmops_bench_run_arg *args;
    if (vmops_utils_prepare_args(cfg, &args)) {
        LOG_ERR("failed to prepare arguments\n");
        return -1;
    }

    plat_thread_fn_t run_fn = bench_run_fn;
    if (vmops_utils_run_benchmark(cfg->corelist_size, args, run_fn)) {
        LOG_ERR("failed to run the benchmark\n");
        return -1;
    }

    vmops_utils_print_csv(args);

    vmops_utils_cleanup_args(args);

    return 0;
}