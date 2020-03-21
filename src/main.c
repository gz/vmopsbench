/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "benchmarks/benchmarks.h"

static struct vmops_bench_cfg cfg = { .memsize = 4096, .coreslist = NULL, .nrep = 10 };

static uint32_t *parse_cores_list(const char *cores)
{
    size_t i = 0;

    /* 1,2,3-5,6 */
    while (cores[i]) {
        i++;
    }

    return NULL;
}

static void print_help(const char *bin)
{
    fprintf(stderr, "Usage: %s [-c coreslist] [-m memsize] [-b benchmark] [-p nproc]...\n", bin);
}

/**
 * @brief main function
 *
 * @param argc  number of arguments supplied
 * @param argv  the argument strings
 *
 * @returns EXIT_SUCCESS on success, EXIT_FAILURE on failure
 */
int main(int argc, char *argv[])
{
    uint32_t ncores = 0;
    const char *benchmark = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "p:n:c:m:b:h")) != -1) {
        switch (opt) {
        case 'p':
            ncores = strtoul(optarg, NULL, 10);
            break;
        case 'c':
            cfg.coreslist = parse_cores_list(optarg);
            break;
        case 'm':
            cfg.memsize = strtoul(optarg, NULL, 10);
            break;
        case 'b':
            benchmark = optarg;
            break;
        case 'n':
            cfg.nrep = strtoul(optarg, NULL, 10);
            break;
        case 'h':
            print_help(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // either the first ncores are selected, or a provided cores list
    if (ncores != 0 && cfg.coreslist != NULL) {
        fprintf(stderr, "ERROR: Please provide either coreslist or number of cores.\n");
        exit(EXIT_FAILURE);
    }

    // validate cores list
    if (cfg.coreslist == NULL) {
        if (ncores == 0) {
            ncores = 1;
        }

        cfg.coreslist = malloc(ncores * sizeof(uint32_t));
        if (cfg.coreslist == NULL) {
            fprintf(stderr, "ERROR: Out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (uint32_t i = 0; i < ncores; i++) {
            cfg.coreslist[i] = i;
        }
    }

    // run the benchmark
    if (strcmp(benchmark, "isolated") == 0) {
        vmops_bench_run_isolated(&cfg);
    } else if (strcmp(benchmark, "independent") == 0) {
        vmops_bench_run_independent(&cfg);
    } else if (strcmp(benchmark, "shared-isolated") == 0) {
        vmops_bench_run_shared_isolated(&cfg);
    } else if (strcmp(benchmark, "shared-independent") == 0) {
        vmops_bench_run_shared_independent(&cfg);
    } else if (strcmp(benchmark, "concurrent-protect") == 0) {
        vmops_bench_run_concurrent_protect(&cfg);
    } else {
        fprintf(stderr, "unsupported benchmarch '%s'\n", benchmark);
    }

    return EXIT_SUCCESS;
}