
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


static void *bench_run_fn(void *st)
{
    plat_error_t err;

    struct vmops_bench_run_arg *args = st;
    struct vmops_bench_cfg *cfg = args->cfg;

    size_t counter = 0;
    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    void *addr;
    if (cfg->isolated) {
        addr = utils_vmops_get_map_address(args->tid);
        err = plat_vm_map_fixed(addr, cfg->memsize, args->memobj, 0, cfg->maphuge);
    } else {
        err = plat_vm_map(&addr, cfg->memsize, args->memobj, 0, cfg->maphuge);
    }

    if (err != PLAT_ERR_OK) {
        LOG_ERR("thread %d failed to map memory. exiting.\n", args->tid);
        return NULL;
    }

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    while (t_current < t_end) {
        err = plat_vm_protect(addr, cfg->memsize, PLAT_PERM_READ_ONLY);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d. failed to protect memory!\n", args->tid);
            goto err_out;
        }
        err = plat_vm_protect(addr, cfg->memsize, PLAT_PERM_READ_WRITE);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
            goto err_out;
        }
        t_current = plat_get_time();
        counter++;
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


static void *bench_run_4k_fn(void *st)
{
    plat_error_t err;

    struct vmops_bench_run_arg *args = st;
    struct vmops_bench_cfg *cfg = args->cfg;

    size_t counter = 0;
    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    size_t nmaps = cfg->memsize / PAGE_SIZE;
    void **addrs = calloc(nmaps, sizeof(void *));
    if (addrs == NULL) {
        LOG_ERR("thread %d malloc failed!\n", args->tid);
        return NULL;
    }

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        for (size_t i = 0; i < nmaps; i++) {
            err = plat_vm_map_fixed(addr, cfg->memsize, args->memobj, i * PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            addrs[i] = addr;

            addr = (void *)((uintptr_t)addr + PAGE_SIZE);
        }
    } else {
        for (size_t i = 0; i < nmaps; i++) {
            err = plat_vm_map(&addrs[i], PAGE_SIZE, args->memobj, i * PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                goto cleanup_and_exit;
            }
        }
    }

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);

    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    size_t page = args->tid;
    while (t_current < t_end) {
        size_t idx = (page++) % nmaps;
        err = plat_vm_protect(addrs[idx], PAGE_SIZE, PLAT_PERM_READ_ONLY);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d. failed to protect memory!\n", args->tid);
            goto cleanup_and_exit;
        }
        err = plat_vm_protect(addrs[idx], PAGE_SIZE, PLAT_PERM_READ_WRITE);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
            goto cleanup_and_exit;
        }
        t_current = plat_get_time();
        counter++;
    }
    t_end = plat_get_time();

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

cleanup_and_exit:

    plat_thread_barrier(args->barrier);

    for (size_t i = 0; i < nmaps; i++) {
        if (addrs[i] != NULL) {
            err = plat_vm_unmap(addrs[i], PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory!\n", args->tid);
            }
        }
    }

    free(addrs);

    plat_thread_barrier(args->barrier);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

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
int vmops_bench_run_protect(struct vmops_bench_cfg *cfg, const char *opts)
{
    if (vmops_utils_parse_options(opts, cfg)) {
        LOG_ERR("failed to parse the options\n");
        return -1;
    }


    LOG_INFO("Preparing benchmark. 'protect' with options '%s'\n", vmops_utils_print_options(cfg));

    struct vmops_bench_run_arg *args;
    if (vmops_utils_prepare_args(cfg, &args)) {
        LOG_ERR("failed to prepare arguments\n");
        return -1;
    }

    plat_thread_fn_t run_fn = bench_run_fn;

    if (cfg->map4k) {
        run_fn = bench_run_4k_fn;
    }

    if (vmops_utils_run_benchmark(cfg->corelist_size, args, run_fn)) {
        LOG_ERR("failed to run the benchmark\n");
        return -1;
    }

    size_t total_ops = 0;
    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        if (args[i].thread != NULL) {
            total_ops += args[i].count;
        }
    }

    LOG_INFO("Benchmark done. total ops = %zu\n", total_ops);
    vmops_utils_print_csv(args, total_ops);

    vmops_utils_cleanup_args(args);

    return 0;
}