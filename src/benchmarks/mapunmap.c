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

struct bench_run_arg
{
    struct vmops_bench_cfg *cfg;
    plat_memobj_t memobj;
    plat_thread_t thread;
    uint32_t tid;
    plat_barrier_t barrier;
    size_t count;
    double duration;
};

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

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    size_t memsize = args->cfg->memsize;
    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_delta == PLAT_TIME_MAX ? PLAT_TIME_MAX : t_current + t_delta;
    plat_time_t t_start = t_current;
    size_t counter = 0;

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end && counter < nops) {
            plat_time_t t_op_start = t_current;
            err = plat_vm_map_fixed(addr, memsize, args->memobj, 0, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory ops=%zu!\n", args->tid, counter);
                return NULL;
            }

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
            plat_time_t t_op_start = t_current;

            void *addr;
            err = plat_vm_map(&addr, memsize, args->memobj, 0, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory ops=%zu!\n", args->tid, counter);
                return NULL;
            }

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

    plat_thread_barrier(args->barrier);

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

    return NULL;
}


static void *bench_run_4k_fn(struct vmops_bench_run_arg *args)
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
    size_t counter = 0;

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

static void *bench_run_nounmap_fn(struct vmops_bench_run_arg *args)
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

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    size_t memsize = args->cfg->memsize;
    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_delta == PLAT_TIME_MAX ? PLAT_TIME_MAX : t_current + t_delta;
    plat_time_t t_start = t_current;
    plat_time_t t_op_start;
    size_t counter = 0;

    if (cfg->isolated) {
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end && counter < nops) {
            t_op_start = t_current;
            err = plat_vm_map_fixed(addr, memsize, args->memobj, 0, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                return NULL;
            }
            addr += memsize;
            t_current = plat_get_time();
            counter++;
            vmops_utils_add_stats(&args->stats, args->tid, counter, t_current - t_start,
                                  t_current - t_op_start);
        }
    } else {
        while (t_current < t_end && counter < nops) {
            t_op_start = t_current;
            void *addr;
            err = plat_vm_map(&addr, memsize, args->memobj, 0, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                return NULL;
            }

            t_current = plat_get_time();
            counter++;
            vmops_utils_add_stats(&args->stats, args->tid, counter, t_current - t_start,
                                  t_current - t_op_start);
        }
    }
    t_end = plat_get_time();

    plat_thread_barrier(args->barrier);

    args->count = counter;
    args->duration = plat_time_to_ms(t_end - t_start);

    LOG_INFO("thread %d done. ops = %zu, time=%.3f\n", args->tid, counter, args->duration);

    return NULL;
}

static void *bench_run_nounmap_4k_fn(struct vmops_bench_run_arg *args)
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

    size_t nmaps = cfg->memsize / PLAT_ARCH_BASE_PAGE_SIZE;

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);


    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_delta == PLAT_TIME_MAX ? PLAT_TIME_MAX : t_current + t_delta;
    plat_time_t t_start = t_current;
    size_t counter = 0;

    if (cfg->isolated) {
        size_t page = args->tid;
        void *addr = utils_vmops_get_map_address(args->tid);
        while (t_current < t_end && counter < nops) {
            size_t idx = (page++) % nmaps;

            err = plat_vm_map_fixed(addr, PLAT_ARCH_BASE_PAGE_SIZE, args->memobj,
                                    idx * PLAT_ARCH_BASE_PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
                goto cleanup_and_exit;
            }
            addr = (void *)((uintptr_t)addr + PLAT_ARCH_BASE_PAGE_SIZE);
            t_current = plat_get_time();
            counter++;
        }
    } else {
        size_t page = args->tid;
        while (t_current < t_end && counter < nops) {
            size_t idx = (page++) % nmaps;
            void *addr;
            err = plat_vm_map(&addr, PLAT_ARCH_BASE_PAGE_SIZE, args->memobj,
                              idx * PLAT_ARCH_BASE_PAGE_SIZE, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory counter=%zu!\n", args->tid, counter);
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

    vmops_utils_print_csv(args);

    vmops_utils_cleanup_args(args);

    return 0;
}