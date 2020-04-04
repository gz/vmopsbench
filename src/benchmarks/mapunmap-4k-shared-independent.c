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

#define VMOBJ_NAME "/vmops_bench_shared_independent"

struct bench_run_arg
{
    struct vmops_bench_cfg *cfg;
    plat_memobj_t memobj;
    plat_thread_t thread;
    uint32_t tid;
    plat_barrier_t barrier;
    size_t count;
};

static void *bench_run_fn(void *st)
{
    plat_error_t err;

    struct bench_run_arg *args = st;

    printf("thread %d started. Running for %d ms.\n", args->tid, args->cfg->time_ms);

    plat_time_t t_delta = plat_convert_time(args->cfg->time_ms);

    size_t counter = 0 ;

    size_t nmaps = (args->cfg->memsize / PAGE_SIZE) * args->cfg->nmaps;
    void **addrs = malloc(nmaps * sizeof(void *));
    if (addrs == NULL) {
        LOG_ERR("malloc failed!\n");
        return NULL;
    }

    plat_thread_barrier(args->barrier);


    plat_time_t t_current = plat_get_time();
    plat_time_t t_end = t_current + t_delta;

    if (args->cfg->nounmap) {

    } else {

        while(t_current < t_end) {
            err = plat_vm_map(&addrs[counter % nmaps], args->cfg->memsize, args->memobj, 0);
            if (err != PLAT_ERR_OK) {
                printf("failed to map!\n");
            }

            err = plat_vm_unmap(addrs[counter % nmaps], args->cfg->memsize);
            if (err != PLAT_ERR_OK) {
                printf("failed to unmap!\n");
            }
            t_current = plat_get_time();
            counter++;
        }
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
void vmops_bench_run_4k_shared_independent(struct vmops_bench_cfg *cfg)
{
    plat_error_t err;

    printf("+ VMOPS Selecting the shared independent configuration.\n");

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

    plat_memobj_t mobj;
    err = plat_vm_create(VMOBJ_NAME, &mobj, cfg->memsize);
    if (err != PLAT_ERR_OK) {
        printf(
            "could not create the memobj\n"
        );
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        args[i].tid = i;
        args[i].barrier = barrier;
        args[i].memobj = mobj;
        args[i].cfg = cfg;
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

    plat_vm_destroy(mobj);

    plat_thread_barrier_destroy(barrier);

    printf("+ VMOPS Benchmark done. ncores=%d, total ops = %zu\n", cfg->corelist_size, total_ops);

    free(args);
}