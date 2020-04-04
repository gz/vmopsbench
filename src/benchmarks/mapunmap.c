/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "benchmarks.h"
#include "utils.h"

#define VMOBJ_NAME "/vmops_bench_mapunmap_independent_%d"

struct bench_run_arg {
    struct vmops_bench_cfg *cfg;
    plat_memobj_t memobj;
    plat_thread_t thread;
    uint32_t tid;
    plat_barrier_t barrier;
    size_t count;
    double duration;
};

static void *bench_run_fn(void *st)
{
    plat_error_t err;

    struct vmops_bench_run_arg *args = st;
    struct vmops_bench_cfg *cfg = args->cfg;

    size_t counter = 0;
    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    size_t memsize = args->cfg->memsize;
    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end) {
            err = plat_vm_map_fixed(addr, memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                return NULL;
            }

            err = plat_vm_unmap(addr, memsize);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory!\n", args->tid);
                return NULL;
            }

            t_current = plat_get_time();
            counter++;
        }
    } else {
        while (t_current < t_end) {
            void *addr;
            err = plat_vm_map(&addr, memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                return NULL;
            }

            err = plat_vm_unmap(addr, memsize);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory!\n", args->tid);
                return NULL;
            }

            t_current = plat_get_time();
            counter++;
        }
    }
    t_end = plat_get_time();

    plat_thread_barrier(args->barrier);

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

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
            err = plat_vm_map_fixed(addr, cfg->memsize, args->memobj, i * PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                goto cleanup_and_exit;
            }

            addr = (void *)((uintptr_t)addr + PAGE_SIZE);
        }
    } else {
        for (size_t i = 0; i < nmaps; i++) {
            err = plat_vm_map(&addrs[i], PAGE_SIZE, args->memobj, i * PAGE_SIZE);
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

    if (cfg->isolated) {
        size_t page = args->tid;
        while (t_current < t_end) {
            size_t idx = (page++) % nmaps;
            err = plat_vm_unmap(addrs[idx], PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to protect memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            err = plat_vm_map_fixed(addrs[idx], PAGE_SIZE, args->memobj, idx * PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            t_current = plat_get_time();
            counter++;
        }
    } else {
        size_t page = args->tid;
        while (t_current < t_end) {
            size_t idx = (page++) % nmaps;
            err = plat_vm_unmap(addrs[idx], PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to protect memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            err = plat_vm_map(&addrs[idx], PAGE_SIZE, args->memobj, idx * PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            t_current = plat_get_time();
            counter++;
        }
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

static void *bench_run_nounmap_fn(void *st)
{
    plat_error_t err;

    struct vmops_bench_run_arg *args = st;
    struct vmops_bench_cfg *cfg = args->cfg;

    size_t counter = 0;
    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    size_t memsize = args->cfg->memsize;
    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end) {
            err = plat_vm_map_fixed(addr, memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                return NULL;
            }
            addr += memsize;
            t_current = plat_get_time();
            counter++;
        }
    } else {
        while (t_current < t_end) {
            void *addr;
            err = plat_vm_map(&addr, memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                return NULL;
            }

            t_current = plat_get_time();
            counter++;
        }
    }
    t_end = plat_get_time();

    plat_thread_barrier(args->barrier);

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

    return NULL;
}

static void *bench_run_nounmap_4k_fn(void *st)
{
    plat_error_t err;

    struct vmops_bench_run_arg *args = st;
    struct vmops_bench_cfg *cfg = args->cfg;

    size_t counter = 0;
    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    size_t nmaps = cfg->memsize / PAGE_SIZE;

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    if (cfg->isolated) {
        size_t page = args->tid;
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end) {
            size_t idx = (page++) % nmaps;

            err = plat_vm_map_fixed(addr, PAGE_SIZE, args->memobj, idx * PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            addr = (void *)((uintptr_t)addr + PAGE_SIZE);
            t_current = plat_get_time();
            counter++;
        }
    } else {
        size_t page = args->tid;
        while (t_current < t_end) {
            size_t idx = (page++) % nmaps;
            void *addr;
            err = plat_vm_map(&addr, PAGE_SIZE, args->memobj, idx * PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unprotect memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            t_current = plat_get_time();
            counter++;
        }
    }

    t_end = plat_get_time();

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

cleanup_and_exit:

    plat_thread_barrier(args->barrier);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

    return NULL;
}

/**
 * @brief starts the maponly or mapunmap benchmark
 *
 * @param cfg   the benchmark configuration
 * @param opts  the options for the benchmark
 *
 * @returns 0 success, -1 error
 */
int vmpos_bench_run_mapunmap(struct vmops_bench_cfg *cfg, const char *opts)
{
    if (vmops_utils_parse_options(opts, cfg)) {
        LOG_ERR("failed to parse the options\n");
        return -1;
    }

    LOG_INFO("Preparing benchmark. 'map/unmap' with options '%s'\n",
             vmops_utils_print_options(cfg));

    struct vmops_bench_run_arg *args;
    if (vmops_utils_prepare_args(cfg, &args)) {
        LOG_ERR("failed to prepare arguments\n");
        return -1;
    }

    plat_thread_fn_t run_fn = bench_run_fn;
    if (cfg->nounmap) {
        if (cfg->map4k) {
            run_fn = bench_run_nounmap_4k_fn;
        } else {
            run_fn = bench_run_nounmap_fn;
        }
    } else {
        if (cfg->map4k) {
            run_fn = bench_run_4k_fn;
        } else {
            run_fn = bench_run_fn;
        }
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