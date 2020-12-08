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

#define VMOBJ_NAME "/vmops_bench_tlbshoot_independent_%d"

static void *bench_run_fn(struct vmops_bench_run_arg *args)
{
    plat_error_t err;

    struct vmops_bench_cfg *cfg = args->cfg;

    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);
    if (t_delta == 0) {
        t_delta = PLAT_TIME_MAX;
    }

    size_t nops = cfg->nops;
    if (nops == 0) {
        nops = SIZE_MAX;
    }
    size_t memsize = args->cfg->memsize;
    void *taddr;
    err = plat_vm_map(&taddr, memsize, args->memobj, 0, cfg->maphuge);
    if (err != PLAT_ERR_OK) {
        LOG_ERR("thread %d. failed to map memory!\n", args->tid);
        return NULL;
    }

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);

    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_delta == PLAT_TIME_MAX ? PLAT_TIME_MAX : t_current + t_delta;
    plat_time_t t_start = t_current;
    size_t counter = 0;

    uint64_t *done = (uint64_t*)args->shared;
    if (args->tid != 0) {
        uint64_t *sum = (uint64_t*)taddr;
        while(!*done) {
            *sum = *sum + 1;
        }
        plat_thread_barrier(args->barrier);

        args->count = counter;
        args->duration = plat_time_to_ms(t_end - t_start);

        LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, *sum, args->duration);

        return NULL;
    }

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end && counter < nops) {
            err = plat_vm_map_fixed(addr, memsize, args->memobj, 0, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory ops=%zu!\n", args->tid, counter);
                return NULL;
            }

            plat_time_t t_op_start = t_current;
            err = plat_vm_unmap(addr, memsize);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory ops=%zu!\n", args->tid, counter);
                return NULL;
            }
            t_current = plat_get_time();

            vmops_utils_add_stats(&args->stats, args->tid, counter, t_current - t_start,
                                  t_current - t_op_start);

            counter++;
        }
    } else {
        while (t_current < t_end && counter < nops) {
            void *addr;
            err = plat_vm_map(&addr, memsize, args->memobj, 0, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory ops=%zu!\n", args->tid, counter);
                return NULL;
            }

            plat_time_t t_op_start = t_current;
            err = plat_vm_unmap(addr, memsize);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory ops=%zu!\n", args->tid, counter);
                return NULL;
            }

            t_current = plat_get_time();
            vmops_utils_add_stats(&args->stats, args->tid, counter, t_current - t_start,
                                  t_current - t_op_start);

            counter++;
        }
    }
    t_end = plat_get_time();

    *done = 0x1;

    plat_thread_barrier(args->barrier);

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

    return NULL;
}

#if 0
static void *bench_run_4k_fn(struct vmops_bench_run_arg *args)
{
    plat_error_t err;

    struct vmops_bench_cfg *cfg = args->cfg;

    size_t counter = 0;


    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);
    if (t_delta == 0) {
        t_delta = PLAT_TIME_MAX;
    }

    size_t nops = cfg->nops;
    if (nops == 0) {
        nops = SIZE_MAX;
    }

    size_t nmaps = cfg->memsize / PLAT_ARCH_BASE_PAGE_SIZE;
    void **addrs = calloc(nmaps, sizeof(void *));
    if (addrs == NULL) {
        LOG_ERR("thread %d malloc failed!\n", args->tid);
        return NULL;
    }

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        for (size_t i = 0; i < nmaps; i++) {
            err = plat_vm_map_fixed(addr, PLAT_ARCH_BASE_PAGE_SIZE, args->memobj,
                                    i * PLAT_ARCH_BASE_PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory i=%zu!\n", args->tid, i);
                goto cleanup_and_exit;
            }
            addrs[i] = addr;

            addr = (void *)((uintptr_t)addr + PLAT_ARCH_BASE_PAGE_SIZE);
        }
    } else {
        for (size_t i = 0; i < nmaps; i++) {
            err = plat_vm_map(&addrs[i], PLAT_ARCH_BASE_PAGE_SIZE, args->memobj,
                              i * PLAT_ARCH_BASE_PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory i=%zu!\n", args->tid, i);
                goto cleanup_and_exit;
            }
        }
    }

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_delta == PLAT_TIME_MAX ? PLAT_TIME_MAX : t_current + t_delta;
    plat_time_t t_start = t_current;

    if (cfg->isolated) {
        size_t page = args->tid;
        while (t_current < t_end && counter < nops) {
            size_t idx = (page++) % nmaps;
            err = plat_vm_unmap(addrs[idx], PLAT_ARCH_BASE_PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory! %p\n", args->tid, addrs[idx]);
                goto cleanup_and_exit;
            }
            err = plat_vm_map_fixed(addrs[idx], PLAT_ARCH_BASE_PAGE_SIZE, args->memobj,
                                    idx * PLAT_ARCH_BASE_PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map fixed memory! %p\n", args->tid, addrs[idx]);
                goto cleanup_and_exit;
            }
            t_current = plat_get_time();
            counter++;
        }
    } else {
        size_t page = args->tid;
        while (t_current < t_end && counter < nops) {
            size_t idx = (page++) % nmaps;
            err = plat_vm_unmap(addrs[idx], PLAT_ARCH_BASE_PAGE_SIZE);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory! %p\n", args->tid, addrs[idx]);
                goto cleanup_and_exit;
            }
            err = plat_vm_map(&addrs[idx], PLAT_ARCH_BASE_PAGE_SIZE, args->memobj,
                              idx * PLAT_ARCH_BASE_PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
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
            err = plat_vm_unmap(addrs[i], PLAT_ARCH_BASE_PAGE_SIZE);
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
#endif

/**
 * @brief starts the tlbshootdown benchmark
 *
 * @param cfg   the benchmark configuration
 * @param opts  the options for the benchmark
 *
 * @returns 0 success, -1 error
 */
int vmops_bench_run_tlbshoot(struct vmops_bench_cfg *cfg, const char *opts)
{
    if (vmops_utils_parse_options(opts, cfg)) {
        LOG_ERR("failed to parse the options\n");
        return -1;
    }

    LOG_INFO("Preparing benchmark. 'map/unmap' with options '%s'\n",
             vmops_utils_print_options(cfg));

    uint64_t done = 0;
    struct vmops_bench_run_arg *args;
    if (vmops_utils_prepare_args(cfg, (void *)&done, &args)) {
        LOG_ERR("failed to prepare arguments\n");
        return -1;
    }


    plat_thread_fn_t run_fn = bench_run_fn;
    if (cfg->map4k) {
        run_fn = bench_run_fn;
    } else {
        run_fn = bench_run_fn;
    }

    if (vmops_utils_run_benchmark(cfg->corelist_size, args, run_fn)) {
        LOG_ERR("failed to run the benchmark\n");
        return -1;
    }

    vmops_utils_print_csv(args);

    vmops_utils_cleanup_args(args);

    return 0;
}