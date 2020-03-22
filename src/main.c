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
#include <ctype.h>

#include "benchmarks/benchmarks.h"

static struct vmops_bench_cfg cfg
    = { .memsize = 4096, .coreslist = NULL, .corelist_size = 0, .time_ms = 10 };


static int parse_cores_list(char *cores, uint32_t **retcoreslist, uint32_t *ncores)
{
    size_t coreslen = strlen(cores);
    if (coreslen == 0) {
        fprintf(stderr, "ERROR: coreslist length was 0\n");
        exit(EXIT_FAILURE);
    }

    if (!isdigit(cores[0])) {
        fprintf(stderr, "ERROR: coreslist needs to start with a digit, was %c\n", cores[0]);
        exit(EXIT_FAILURE);
    }

    char *current = cores;

    size_t corecount = 1;
    while (*current) {
        if (*current == ',') {
            corecount++;
        }
        current++;
    }

    uint32_t *coreslist = malloc(corecount * sizeof(uint32_t));
    if (coreslist == NULL) {
        return -1;
    }

    current = cores;
    char *lookahead = cores;

    size_t idx = 0;
    while (current < cores + coreslen) {
        while (isdigit(*lookahead) && *lookahead != 0) {
            lookahead++;
        }

        if (*lookahead != 0 && *lookahead != ',')  {
            fprintf(stderr, "ERROR: expected a ',' was '%c'\n", *lookahead);
            exit(EXIT_FAILURE);
        }

        if (lookahead == current) {
            fprintf(stderr, "ERROR: expected a digit was '%c'\n", *lookahead);
            exit(EXIT_FAILURE);
        }

        *lookahead = 0;

        uint32_t cid = strtoul(current, NULL, 10);
        coreslist[idx++] = cid;

        lookahead++;
        current = lookahead;
    }

    *retcoreslist = coreslist;
    *ncores = corecount;

    return 0;
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
    plat_error_t err;

    uint32_t ncores = 0;
    const char *benchmark = "independent";


    plat_topo_numa_t numa_topology = PLAT_TOPOLOGY_NUMA_FILL;
    plat_topo_cores_t cores_topology = PLAT_TOPOLOGY_CORES_INTERLEAVE;

    printf("+ VMOPS Benchmark\n");

    int opt;
    while ((opt = getopt(argc, argv, "ip:t:c:m:b:h")) != -1) {
        switch (opt) {
        case 'p':
            ncores = strtoul(optarg, NULL, 10);
            break;
        case 'c':
            parse_cores_list(optarg, &cfg.coreslist, &cfg.corelist_size);
            break;
        case 'i' :
            numa_topology = PLAT_TOPOLOGY_NUMA_INTERLEAVE;
            break;
        case 'm':
            cfg.memsize = strtoul(optarg, NULL, 10);
            break;
        case 'b':
            benchmark = optarg;
            break;
        case 't':
            cfg.time_ms = strtoul(optarg, NULL, 10);
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

    plat_init();

    // validate cores list
    if (cfg.coreslist == NULL) {
        if (ncores == 0) {
            ncores = 1;
        }

        err = plat_get_topology(numa_topology, cores_topology, &cfg.coreslist, &cfg.corelist_size);
        if (err != PLAT_ERR_OK) {
            fprintf(stderr, "ERROR: Could not get the core list.\n");
            exit(EXIT_FAILURE);
        }

        if (ncores > cfg.corelist_size) {
            fprintf(stderr, "WARNING: Requested more cores thatn there are available.\n");
        } else {
            cfg.corelist_size = ncores;
        }
    }

    printf("Using Cores: [ %d", cfg.coreslist[0]);
    for (uint32_t i = 1; i < cfg.corelist_size; i++) {
        printf(", %d", cfg.coreslist[i]);
    }
    printf(" ]\n");

    // run the benchmark
    if (strcmp(benchmark, "isolated") == 0) {
        vmops_bench_run_isolated(&cfg);
    } else if (strcmp(benchmark, "independent") == 0) {
        vmops_bench_run_independent(&cfg);
    } else if (strcmp(benchmark, "shared-isolated") == 0) {
        vmops_bench_run_shared_isolated(&cfg);
    } else if (strcmp(benchmark, "shared-independent") == 0) {
        vmops_bench_run_shared_independent(&cfg);
    } else if (strcmp(benchmark, "protect-shared") == 0) {
        vmops_bench_run_protect_shared(&cfg);
    } else if (strcmp(benchmark, "protect-independent") == 0) {
        vmops_bench_run_protect_independent(&cfg);
    } else {
        fprintf(stderr, "unsupported benchmarch '%s'\n", benchmark);
    }


    free(cfg.coreslist);

    return EXIT_SUCCESS;
}