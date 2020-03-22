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

#define ADDRESS_OFFSET (512UL << 30)

#define VMOBJ_NAME "/vmops_bench_solated_%d"

struct bench_run_arg
{
    plat_memobj_t memobj;
    plat_thread_t thread;
    size_t memsize;
    uint32_t time_ms;
    uint32_t tid;
    plat_barrier_t barrier;
    size_t count;
};

static void *bench_run_fn(void *st)
{
    plat_error_t err;

    struct bench_run_arg *args = st;

    printf("thread %d started. Running for %d ms.\n", args->tid, args->time_ms);

    plat_time_t t_delta = plat_convert_time(args->time_ms);

    size_t counter = 0 ;

    plat_thread_barrier(args->barrier);


    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;

    while(t_current < t_end) {

        void *addr = (void *)(ADDRESS_OFFSET * (args->tid + 1));
        err = plat_vm_map_fixed(addr, args->memsize, args->memobj, 0);
        if (err != PLAT_ERR_OK) {
            printf("failed to map!\n");
        }

        err = plat_vm_unmap(addr, args->memsize);
        if (err != PLAT_ERR_OK) {
            printf("failed to unmap!\n");
        }
        t_current = plat_get_time();
        counter++;
    }

    plat_thread_barrier(args->barrier);

    printf("thread %d ended. %zu map + unmaps\n", args->tid, counter);

    args->count = counter;

    return NULL;
}


/**
 * @brief runs the vmops benchmark in the concurrent protect configuration
 *
 * @param cfg   the benchmark configuration
 *
 *  - there is a single shared memory region
 *  - calling randomly protect() on pages.
 */
void vmops_bench_run_isolated(struct vmops_bench_cfg *cfg)
{
    plat_error_t err;

    printf("+ VMOPS Selecting the isolated configuration.\n");

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

    // buffer for the path
    char pathbuf[256];

    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        snprintf(pathbuf, sizeof(pathbuf), VMOBJ_NAME, cfg->coreslist[i]);
        err = plat_vm_create(pathbuf, &args[i].memobj, cfg->memsize);
        if (err != PLAT_ERR_OK) {
            for (uint32_t j = 0; j < i; j++) {
                plat_vm_destroy(args[j].memobj);
            }
            exit(EXIT_FAILURE);
        }
    }

    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        args[i].tid = i;
        args[i].memsize = cfg->memsize;
        args[i].time_ms = cfg->time_ms;
        args[i].barrier = barrier;
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

    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        plat_vm_destroy(args[i].memobj);
    }

    plat_thread_barrier_destroy(barrier);

    printf("+ VMOPS Benchmark done. ncores=%d, total ops = %zu\n", cfg->corelist_size, total_ops);

    free(args);
}