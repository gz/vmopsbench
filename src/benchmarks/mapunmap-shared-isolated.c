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

#define VMOBJ_NAME "/vmops_bench_shared_isolated"

#define ADDRESS_OFFSET (512UL << 31)

struct bench_run_arg {
    plat_memobj_t memobj;
    plat_thread_t thread;
    struct vmops_bench_cfg *cfg;
    uint32_t tid;
    plat_barrier_t barrier;
    size_t count;
    double duration;
};

static void *bench_run_fn(void *st)
{
    plat_error_t err;

    struct bench_run_arg *args = st;

    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    size_t counter = 0;

    LOG_INFO("thread %d ready.\n", args->tid);
    plat_thread_barrier(args->barrier);

    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;
    plat_time_t t_start = t_current;

    if (args->cfg->nounmap) {
        void *addr = (void *)(ADDRESS_OFFSET * (args->tid + 1));
        void *addr_end = (void *)(ADDRESS_OFFSET * (args->tid + 2));
        while (t_current < t_end) {
            err = plat_vm_map_fixed(addr, args->cfg->memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory! counter = %zu, %p\n", args->tid, counter,
                        addr);
                exit(1);
            }
            counter++;
            addr = (void *)((uintptr_t)addr + args->cfg->memsize);
            if (addr > addr_end) {
                LOG_ERR("error %d. ran into region of other thread!\n", args->tid);
            }
            t_current = plat_get_time();
        }
    } else {
        while (t_current < t_end) {
            void *addr = (void *)(ADDRESS_OFFSET * (args->tid + 1));
            err = plat_vm_map_fixed(addr, args->cfg->memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to map memory!\n", args->tid);
            }

            err = plat_vm_unmap(addr, args->cfg->memsize);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("thread %d. failed to unmap memory!\n", args->tid);
                exit(1);
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

/**
 * @brief runs the vmops benchmark in the shared-isolated configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - the memory object is shared
 *  - each thread maps it in a distinct slot in the root page table
 */
void vmops_bench_run_shared_isolated(struct vmops_bench_cfg *cfg)
{
    plat_error_t err;

    if (cfg->nounmap) {
        LOG_INFO("Preparing 'MAPONLY Shared' benchmark.\n");
    } else {
        LOG_INFO("Preparing 'MAP/UNMAP Shared' benchmark.\n");
    }

    /* create barrier */
    plat_barrier_t barrier;
    err = plat_thread_barrier_init(&barrier, cfg->corelist_size);
    if (err != PLAT_ERR_OK) {
        exit(EXIT_FAILURE);
    }

    /* create the threads */
    struct bench_run_arg *args = malloc(cfg->corelist_size * sizeof(struct bench_run_arg));
    if (args == NULL) {
        exit(EXIT_FAILURE);
    }

    LOG_INFO("creating %d memory objects of size %zu\n", cfg->corelist_size, cfg->memsize);
    LOG_INFO("total memory usage = %zu kB\n", (cfg->corelist_size * cfg->memsize) >> 10);

    plat_memobj_t mobj;
    err = plat_vm_create(VMOBJ_NAME, &mobj, cfg->memsize);
    if (err != PLAT_ERR_OK) {
        LOG_ERR("could not create the memobj\n");
        exit(EXIT_FAILURE);
    }

    LOG_INFO("creating %d threads\n", cfg->corelist_size);
    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        args[i].tid = i;
        args[i].cfg = cfg;
        args[i].barrier = barrier;
        args[i].memobj = mobj;
        args[i].thread = plat_thread_start(bench_run_fn, &args[i], cfg->coreslist[i]);
        if (args[i].thread == NULL) {
            for (uint32_t j = 0; j < i; j++) {
                plat_thread_cancel(args[j].thread);
            }
        }
    }

    size_t total_ops = 0;
    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        if (args[i].thread != NULL) {
            plat_thread_join(args[i].thread);
            total_ops += args[i].count;
        }
    }

    LOG_INFO("cleaning up memory object\n");
    plat_vm_destroy(mobj);

    LOG_INFO("Benchmark done. total ops = %zu\n", total_ops);

    LOG_CSV_HEADER();
    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        LOG_CSV(cfg->benchmark, i, cfg->coreslist[i], cfg->corelist_size, cfg->memsize,
                args[i].duration, args[i].count);
    }
    LOG_CSV_FOOTER();
    LOG_RESULT(cfg->benchmark, cfg->memsize, cfg->time_ms, cfg->corelist_size, total_ops);

    plat_thread_barrier_destroy(barrier);

    free(args);
}