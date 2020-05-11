/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */

#include <string.h>

#include "../platform/platform.h"
#include "../logging.h"
#include "utils.h"

#define VMOBJ_NAME_SHARED "/vmops_bench_shared"
#define VMOBJ_NAME_INDEPENDENT "/vmops_bench_independent_%d"


/*
 * ================================================================================================
 * Option Parsing and Printing
 * ================================================================================================
 */

/**
 * @brief parses the benchmark options
 *
 * @param opts  the options string
 * @param cfg   the benchmark config struct
 *
 * @returns 0 on success,  -1 on failure
 */
int vmops_utils_parse_options(const char *opts, struct vmops_bench_cfg *cfg)
{
    const char *current = opts;

    bool shared = false;
    bool independent = false;
    bool isolated = false;
    bool map4k = false;

    while (*current) {
        if (strncmp(current, "-shared", 7) == 0) {
            current += 7;
            shared = true;
        } else if (strncmp(current, "-independent", 12) == 0) {
            current += 12;
            independent = true;
        } else if (strncmp(current, "-4k", 3) == 0) {
            current += 3;
            map4k = true;
        } else if (strncmp(current, "-isolated", 9) == 0) {
            current += 9;
            isolated = true;
        } else {
            LOG_ERR("unknown option '%s'\n", current);
            return -1;
        }
    }

    if (shared && independent) {
        LOG_ERR("cannot enable options 'shared' and 'independent'\n");
        return -1;
    }

    if (!(shared || independent)) {
        shared = true;
    }

    cfg->shared = shared;
    cfg->isolated = isolated;
    cfg->map4k = map4k;

    return 0;
}


/**
 * @brief prints the benchmark options
 *
 * @param cfg   the benchmark config
 *
 * @returns formatted string with the options
 */
char *vmops_utils_print_options(struct vmops_bench_cfg *cfg)
{
    static char optbuf[256];

    snprintf(optbuf, sizeof(optbuf), "%s%s, %s, %s", cfg->nounmap ? "nounmap, " : "",
             cfg->shared ? "shared" : "independent", cfg->isolated ? "isolated" : "default",
             cfg->map4k ? "many 4k mappings" : "one large mapping");

    return optbuf;
}


/*
 * ================================================================================================
 * Result Printing
 * ================================================================================================
 */

static int paircmp(const void *_p1, const void *_p2)
{
    const struct statval *p1 = _p1;
    const struct statval *p2 = _p2;

    if (p1->t_elapsed < p2->t_elapsed) {
        return -1;
    } else if (p1->t_elapsed == p2->t_elapsed) {
        return 0;
    } else {
        return 1;
    }
}


/**
 * @brief prints the results of the benchmark as CSV
 *
 * @param args          the benchmark arguments
 * @param total_ops     the total number of operations executed
 */
void vmops_utils_print_csv(struct vmops_bench_run_arg *args)
{
    if (args == NULL || args->cfg == NULL) {
        return;
    }

    struct vmops_bench_cfg *cfg = args->cfg;

    size_t total_ops = 0;
    double total_time = 0;

    LOG_CSV_HEADER();
    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        if (cfg != args[i].cfg) {
            LOG_ERR("config pointer was not the same! (%p, %p)\n", cfg, args[i].cfg);
            exit(EXIT_FAILURE);
        }

        LOG_CSV(cfg, i, args[i].duration, args[i].count);

        total_ops += args[i].count;
        total_time += args[i].duration;
    }
    LOG_CSV_FOOTER();
    LOG_RESULT(cfg->benchmark, cfg->memsize, total_time, cfg->corelist_size, total_ops,
               (float)(total_ops * 1000) / total_time);

    struct statval *pairs = args[0].stats.values;
    if (cfg->stats && pairs) {
        qsort(pairs, cfg->stats * cfg->corelist_size, sizeof(struct statval), paircmp);

        LOG_STATS_HEADER();
        for (size_t i = 0; i < cfg->stats * cfg->corelist_size; i++) {
            LOG_STATS(i, pairs[i]);
        }
    }
}


/*
 * ================================================================================================
 * Benchmark Argument Preparation
 * ================================================================================================
 */


/**
 * @brief prepares the arguments for teh benchmark threads
 *
 * @param cfg       the benchmark configs
 * @param retargs   returns the thread arguments array
 *
 * @returns 0 on success, -1 on failure
 */
int vmops_utils_prepare_args(struct vmops_bench_cfg *cfg, struct vmops_bench_run_arg **retargs)
{
    plat_error_t err;

    /* create the threads */
    struct vmops_bench_run_arg *args = calloc(cfg->corelist_size + 1,
                                              sizeof(struct vmops_bench_run_arg));
    if (args == NULL) {
        return -1;
    }

    struct statval *vals = NULL;
    if (cfg->stats) {
        LOG_INFO("allocating stats. %zu kB memory\n",
                 (cfg->corelist_size * cfg->stats * sizeof(struct statval)) >> 10);
        vals = calloc(cfg->corelist_size, cfg->stats * sizeof(struct statval));
        if (vals == NULL) {
            LOG_WARN("disabling statistics. failed to get memory!\n");
            cfg->stats = 0;
        }
    }

    size_t totalmem = cfg->shared ? cfg->memsize : cfg->corelist_size * cfg->memsize;
    size_t totalmemobjs = cfg->shared ? 1 : cfg->corelist_size;

    LOG_INFO("creating %zu memory objects of size %zu\n", totalmemobjs, cfg->memsize);
    LOG_INFO("total memory usage = %zu kB\n", totalmem >> 10);

    if (cfg->shared) {
        plat_memobj_t memobj;
        err = plat_vm_create(VMOBJ_NAME_SHARED, &memobj, cfg->memsize, cfg->maphuge);
        if (err != PLAT_ERR_OK) {
            LOG_ERR("creation of shared memory object failed!\n");
            goto err_out;
        }
        for (uint32_t i = 0; i < cfg->corelist_size; i++) {
            args[i].memobj = memobj;
        }
    } else {
        static char pathbuf[256];

        for (uint32_t i = 0; i < cfg->corelist_size; i++) {
            snprintf(pathbuf, sizeof(pathbuf), VMOBJ_NAME_INDEPENDENT, cfg->coreslist[i]);
            err = plat_vm_create(pathbuf, &args[i].memobj, cfg->memsize, cfg->maphuge);
            if (err != PLAT_ERR_OK) {
                LOG_ERR("creation of shared memory object failed! [%d / %d]\n", i,
                        cfg->corelist_size);
                for (uint32_t j = 0; j < i; j++) {
                    plat_vm_destroy(args[i].memobj);
                }
                goto err_out;
            }
        }
    }

    for (uint32_t i = 0; i < cfg->corelist_size; i++) {
        args[i].tid = i;
        args[i].cfg = cfg;
        args[i].coreid = cfg->coreslist[i];
        args[i].stats.sampling_delta = plat_convert_time(cfg->rate);
        args[i].stats.sampling_next = 0;
        args[i].stats.idx_max = cfg->stats;
        args[i].stats.values = cfg->stats > 0 ? vals + cfg->stats * i : NULL;
    }

    args[cfg->corelist_size].tid = -1;

    *retargs = args;

    return 0;

err_out:
    free(args);
    return -1;
}


/**
 * @brief cleans up the previously prepared arguments
 *
 * @param args      the benchmark thread arguments
 *
 * @returns 0 on success, -1 on failure
 */
int vmops_utils_cleanup_args(struct vmops_bench_run_arg *args)
{
    if (args == NULL || args->cfg == NULL) {
        return -1;
    }

    plat_memobj_t memobj = NULL;
    struct vmops_bench_cfg *cfg = args->cfg;

    struct vmops_bench_run_arg *current = args;

    while (current->tid != (uint32_t)-1) {
        LOG_INFO("cleaning up args for thread %d\n", current->tid);
        if (current->memobj != memobj) {
            plat_vm_destroy(current->memobj);
            memobj = current->memobj;
        }

        if (current->cfg != cfg) {
            LOG_ERR("invalid thread arguments structure!\n");
        }

        current++;
    }

    LOG_INFO("cleanup done.\n");

    free(args);

    return 0;
}


/*
 * ================================================================================================
 * Benchmark Running
 * ================================================================================================
 */


/**
 * @brief generic run function for the benchmark threads
 *
 * @param nthreads  number of arguments
 * @param args      the arguments for the threads
 * @param runfn     the function to be run
 *
 * @returns 0 on success, -1 on failure
 */
int vmops_utils_run_benchmark(uint32_t nthreads, struct vmops_bench_run_arg *args,
                              plat_thread_fn_t runfn)
{
    plat_error_t err;

    /* initialize barrier */
    plat_barrier_t barrier;
    err = plat_thread_barrier_init(&barrier, nthreads);
    if (err != PLAT_ERR_OK) {
        LOG_ERR("failed to initialize barrier\n");
        return -1;
    }

    LOG_INFO("creating %d threads\n", nthreads);
    for (uint32_t i = 0; i < nthreads; i++) {
        LOG_INFO("thread %d on core %d\n", args[i].tid, args[i].coreid);
        args[i].barrier = barrier;
        args[i].thread = plat_thread_start(runfn, &args[i], args[i].coreid);
        if (args[i].thread == NULL) {
            LOG_ERR("failed to start threads! [%d / %d]\n", i, nthreads);
            for (uint32_t j = 0; j < i; j++) {
                plat_thread_cancel(args[j].thread);
            }
            return -1;
        }
    }

    for (uint32_t i = 0; i < nthreads; i++) {
        if (args[i].thread != NULL) {
            plat_thread_join(args[i].thread);
        }
    }

    plat_thread_barrier_destroy(barrier);

    return 0;
}