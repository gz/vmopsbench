/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> /* For O_* constants */
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <numa.h>

#include <linux/memfd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */


#include "platform.h"
#include "../logging.h"


#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)

/*
 * ================================================================================================
 * Platform Initialization
 * ================================================================================================
 */


/**
 * @brief initializes the platform backend
 *
 * @param cfg  the benchmark configuration
 *
 * @returns error value
 */
plat_error_t plat_init(struct vmops_bench_cfg *cfg)
{
    (void)cfg;
    LOG_PRINT("Initializing VMOPS bench on Linux\n");
    LOG_INFO("hint: reserve hugepages '/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'\n");
    LOG_INFO("hint: allow more mappings 'sysctl -w vm.max_map_count=2000000000'\n");

    return PLAT_ERR_OK;
}


/*
 * ================================================================================================
 * Platform Topology
 * ================================================================================================
 */


static plat_error_t get_topolocy_no_numa(uint32_t **coreids, uint32_t *ncoreids)
{
    uint32_t ncid = get_nprocs();

    LOG_WARN("seleciting cores using sequential fallback. nproc = %d\n", ncid);

    uint32_t *cids = malloc(ncid * sizeof(uint32_t));
    if (cids == NULL) {
        return PLAT_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < ncid; i++) {
        cids[i] = i;
    }

    *coreids = cids;
    *ncoreids = ncid;

    return PLAT_ERR_OK;
}


/**
 * @brief gets the platform topology for a specific setting
 *
 * @param numapolicy    the NUMA topology filling pattern
 * @param corepolicy    the core topology filling pattern
 * @param coreids       array of core ids to be used, ordered by supplied numa/cores
 * @param ncoreids      the returned number of core ids
 *
 * @returns error value
 */
plat_error_t plat_get_topology(plat_topo_numa_t numapolicy, plat_topo_cores_t corepolicy,
                               uint32_t **coreids, uint32_t *ncoreids)
{
    if (ncoreids == NULL || coreids == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    if (numa_available() == -1) {
        LOG_WARN("NUMA not available!\n");
        return get_topolocy_no_numa(coreids, ncoreids);
    }

    uint32_t nproc = numa_num_task_cpus();
    uint32_t nnodes = numa_num_task_nodes();

    LOG_INFO("seleciting cores from NUMA topology. nodes=%d, nproc=%d\n", nnodes, nproc);

    struct bitmask **nodecpus = malloc(nnodes * sizeof(struct bitmask *));
    if (nodecpus == NULL) {
        return PLAT_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < nnodes; i++) {
        nodecpus[i] = numa_allocate_cpumask();
        if (nodecpus[i] == NULL) {
            for (uint32_t j = 0; j < i; j++) {
                numa_free_cpumask(nodecpus[j]);
            }
            free(nodecpus);
            return PLAT_ERR_NO_MEM;
        }

        if (numa_node_to_cpus(i, nodecpus[i])) {
            LOG_WARN("could not get the CPUs of the node!\n");
            goto err_out_1;
        }
    }

    if (nproc != (uint32_t)get_nprocs()) {
        fprintf(stderr, "WARNING: \n");
        LOG_WARN("numa_num_task_cpus=%d != nproc=%d\n", nproc, get_nprocs());
    }

    uint32_t *cids = malloc(nproc * sizeof(uint32_t));
    uint32_t cidx = 0;

    (void)(corepolicy);

    switch (numapolicy) {
    case PLAT_TOPOLOGY_NUMA_FILL:
        LOG_INFO("using NUMA fill policy. Ignoring hyperthread policy.\n");
        for (uint32_t n = 0; n < nnodes; n++) {
            for (uint32_t c = 0; c < nproc; c++) {
                if (numa_bitmask_isbitset(nodecpus[n], c)) {
                    cids[cidx++] = c;
                }
            }
        }
        break;
    case PLAT_TOPOLOGY_NUMA_INTERLEAVE:
        LOG_INFO("using NUMA interleave policy. Ignoring hyperthread policy.\n");
        for (uint32_t n = 0; n < nnodes; n++) {
            cidx = 0;
            for (uint32_t c = 0; c < nproc; c++) {
                if (numa_bitmask_isbitset(nodecpus[n], c)) {
                    cids[n + (cidx++) * nnodes] = c;
                }
            }
        }
        break;
    default:
        return PLAT_ERR_ARGS_INVALID;
    }

    *coreids = cids;
    *ncoreids = nproc;

    return PLAT_ERR_OK;

err_out_1:
    for (uint32_t j = 0; j < nnodes; j++) {
        numa_free_cpumask(nodecpus[j]);
    }
    free(nodecpus);
    return get_topolocy_no_numa(coreids, ncoreids);
}

/*
 * ================================================================================================
 * Virtual Memory Operations
 * ================================================================================================
 */

///< holds the information about a memory object
struct plat_memobj {
    size_t size;
    int fd;       ///< the file descriptor
    char name[];  ///< the name of the memory object
};


/**
 * @brief creates a memory object for the benchmark
 *
 * @param path      the name/path of the memobj
 * @param memobj    returns a pointer to the created memory object
 * @param size      the maximum size for the memory object
 * @param huge      use huge pages for this memory object
 *
 * @returns error value
 */
plat_error_t plat_vm_create(const char *path, plat_memobj_t *memobj, size_t size, bool huge)
{
    plat_error_t err = PLAT_ERR_OK;

    if (memobj == NULL || path == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    struct plat_memobj *plat_mobj = malloc(sizeof(struct plat_memobj) + strlen(path) + 1);
    if (plat_mobj == NULL) {
        return PLAT_ERR_NO_MEM;
    }

    int flags = 0;
    if (huge) {
        flags |= MFD_HUGETLB | MFD_HUGE_2MB;
    }

    int fd = -1;
    fd = memfd_create(path, flags);
    if (fd == -1) {
        if (errno == EEXIST) {
            LOG_ERR("shm file '%s' already exist. delete it in /dev/shm\n", path);
        }
        err = PLAT_ERR_MEMOBJ_CREATE;
        goto err_out_1;
    }

    /* truncate the file to a given size */
    if (ftruncate(fd, size) != 0) {
        err = PLAT_ERR_NO_MEM;
        goto err_out_2;
    }

    strcpy(plat_mobj->name, path);
    plat_mobj->fd = fd;
    plat_mobj->size = size;

    *memobj = (plat_memobj_t)plat_mobj;

    return PLAT_ERR_OK;

err_out_2:
    shm_unlink(path);

err_out_1:
    free(plat_mobj);

    return err;
}


/**
 * @brief destroys a created memory object
 *
 * @param memobj    the memory object to be destroyed
 *
 * @returns error value
 */
plat_error_t plat_vm_destroy(plat_memobj_t memobj)
{
    struct plat_memobj *plat_mobj = (struct plat_memobj *)memobj;

    if (plat_mobj->fd != 0) {
        // if (shm_unlink(plat_mobj->name)) {
        //     LOG_WARN("could not unline '%s'. Continuing anyway.\n", plat_mobj->name);
        // }
    }
    memset(plat_mobj, 0, sizeof(struct plat_memobj));
    free(plat_mobj);

    return PLAT_ERR_OK;
}


/**
 * @brief maps a region of the memory object
 *
 * @param addr      the returned address where this memory has been mapped
 * @param size      the size of the mapping to be created
 * @param memobj    the backing memory object for this mapping
 * @param offset    the offset into the memory object
 * @param huge      use a huge page mapping
 *
 * @returns returned error value
 */
plat_error_t plat_vm_map(void **addr, size_t size, plat_memobj_t memobj, off_t offset, bool huge)
{
    struct plat_memobj *plat_mobj = (struct plat_memobj *)memobj;
    if (plat_mobj->fd == 0 || plat_mobj->size < offset + size) {
        return PLAT_ERR_ARGS_INVALID;
    }

    if (addr == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    int flags = MAP_SHARED | MAP_POPULATE;

    if (huge) {
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }

    void *map_addr;
    map_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, plat_mobj->fd, offset);
    if (map_addr == MAP_FAILED) {
        return PLAT_ERR_MAP_FAILED;
    }

    *addr = map_addr;

    return PLAT_ERR_OK;
}


/**
 * @brief maps a region of the memory object at fixed address
 *
 * @param addr      the address where this memory has to be mapped
 * @param size      the size of the mapping to be created
 * @param memobj    the backing memory object for this mapping
 * @param offset    the offset into the memory object
 * @param huge      use a huge page mapping
 *
 * @returns returned error value
 */
plat_error_t plat_vm_map_fixed(void *addr, size_t size, plat_memobj_t memobj, off_t offset,
                               bool huge)
{
    struct plat_memobj *plat_mobj = (struct plat_memobj *)memobj;
    if (plat_mobj->fd == 0 || plat_mobj->size < offset + size) {
        return PLAT_ERR_ARGS_INVALID;
    }

    if (addr == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    int flags = MAP_SHARED | MAP_POPULATE | MAP_FIXED;

    if (huge) {
        flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }

    void *map_addr;
    map_addr = mmap(addr, size, PROT_READ | PROT_WRITE, flags, plat_mobj->fd, offset);
    if (map_addr == MAP_FAILED) {
        LOG_ERR("Error = (%d) %s \n", errno, strerror(errno));
        return PLAT_ERR_MAP_FAILED;
    }

    if (map_addr != addr) {
        return PLAT_ERR_MAP_FAILED;
    }

    return PLAT_ERR_OK;
}


/**
 * @brief changes the permissios of a mapping
 *
 * @param addr      the address to protect
 * @param size      the size of the memory region to protect
 * @param perms     the new permissins to set for the region
 *
 * @returns error value
 */
plat_error_t plat_vm_protect(void *addr, size_t size, plat_perm_t perms)
{
    int prot = PROT_READ;
    if (perms == PLAT_PERM_READ_WRITE) {
        prot |= PROT_WRITE;
    }

    if (mprotect(addr, size, prot)) {
        return PLAT_ERR_PROTECT_FAILED;
    }

    return PLAT_ERR_OK;
}


/**
 * @brief unmaps a previously mapped memory region
 *
 * @param addr      the address to be unmapped
 * @param size      the size of the region to be unmapped
 *
 * @returns error value
 */
plat_error_t plat_vm_unmap(void *addr, size_t size)
{
    if (munmap(addr, size)) {
        return PLAT_ERR_UNMAP_FAILED;
    }
    return PLAT_ERR_OK;
}


/*
 * ================================================================================================
 * Threading Functions
 * ================================================================================================
 */


struct plat_thread {
    pthread_t thread;
    uint32_t coreid;
    cpu_set_t cpuset;
    plat_thread_fn_t run;
    struct vmops_bench_run_arg *st;
};


static void *plat_thread_run_fn(void *st)
{
    struct plat_thread *thr = (struct plat_thread *)st;

    CPU_ZERO(&thr->cpuset);
    CPU_SET(thr->coreid, &thr->cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(thr->cpuset), &thr->cpuset);

    thr->run(thr->st);
    return NULL;
}


/**
 * @brief creates and starts a new thread
 *
 * @param run       function to be executed on the thread
 * @param st        state to be passed to the function
 * @param coreid    the core to run the thread on
 *
 * @returns handle to the thread, or NULL on error
 */
plat_thread_t plat_thread_start(plat_thread_fn_t run, struct vmops_bench_run_arg *st,
                                uint32_t coreid)
{
    struct plat_thread *thread = malloc(sizeof(struct plat_thread));
    if (thread == NULL) {
        return NULL;
    }

    thread->coreid = coreid;
    thread->run = run;
    thread->st = st;

    if (pthread_create(&thread->thread, NULL, plat_thread_run_fn, thread)) {
        free(thread);
        return 0;
    }

    return (plat_thread_t)thread;
}


/**
 * @brief cancels and frees up a already created thread
 *
 * @param thread    the thread to be cancelled
 *
 * @returns error value
 */
plat_error_t plat_thread_cancel(plat_thread_t thread)
{
    struct plat_thread *platthread = (struct plat_thread *)thread;


    pthread_cancel(platthread->thread);

    memset(platthread, 0, sizeof(*platthread));
    free(platthread);

    return PLAT_ERR_OK;
}

/**
 * @brief initializes a platform barrier
 *
 * @param barrier   the barrier to initialize
 * @param nthreads  the number of threads to expect
 *
 * @returns error value
 */
plat_error_t plat_thread_barrier_init(plat_barrier_t *barrier, uint32_t nthreads)
{
    pthread_barrier_t *pbar = malloc(sizeof(pthread_barrier_t));
    if (pbar == NULL) {
        return PLAT_ERR_NO_MEM;
    }

    if (pthread_barrier_init(pbar, NULL, nthreads)) {
        free(pbar);
        return PLAT_ERR_BARRIER;
    }

    *barrier = (plat_barrier_t)pbar;

    return PLAT_ERR_OK;
}


/**
 * @brief destroys an initalized platform barrier
 *
 * @param barrier   the barrier to be destroyed
 *
 * @returns error value
 */
plat_error_t plat_thread_barrier_destroy(plat_barrier_t barrier)
{
    pthread_barrier_t *pbar = (pthread_barrier_t *)barrier;
    if (pthread_barrier_destroy(pbar)) {
        return PLAT_ERR_BARRIER;
    }

    return PLAT_ERR_OK;
}


/**
 * @brief calls a barrier to ensure synchronization among the threads
 *
 * @param barrier   the barrier to enter
 *
 * @returns error value
 */
plat_error_t plat_thread_barrier(plat_barrier_t barrier)
{
    pthread_barrier_t *pbar = (pthread_barrier_t *)barrier;
    if (pthread_barrier_wait(pbar)) {
        return PLAT_ERR_BARRIER;
    }

    return PLAT_ERR_OK;
}


/**
 * @brief waits another thread to join
 *
 * @param other     the other thread to be joined
 *
 * @returns return value of the other therad
 */
plat_error_t plat_thread_join(plat_thread_t other)
{
    struct plat_thread *platthread = (struct plat_thread *)other;
    void *retval;
    pthread_join(platthread->thread, &retval);

    memset(platthread, 0, sizeof(*platthread));
    free(platthread);

    return PLAT_ERR_OK;
}


/*
 * ================================================================================================
 * Timing functions
 * ================================================================================================
 */


static inline uint64_t rdtsc(void)
{
    uint32_t eax, edx;
    __asm volatile("rdtsc" : "=a"(eax), "=d"(edx)::"memory");
    return ((uint64_t)edx << 32) | eax;
}

static inline uint64_t rdtscp(void)
{
    uint32_t eax, edx;
    __asm volatile("rdtscp" : "=a"(eax), "=d"(edx)::"ecx", "memory");
    return ((uint64_t)edx << 32) | eax;
}


/**
 * @brief reads the time time
 *
 * @returns time as a plat_time_t
 */
plat_time_t plat_get_time(void)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (uint64_t)(t.tv_sec * 1000000000UL + t.tv_nsec);
}


/**
 * @brief converts time in milliseconds to
 *
 * @param ms    the time in ms to convert to platform time
 *
 * @returns converted time in plat_time_t
 */
plat_time_t plat_convert_time(uint32_t ms)
{
    return (plat_time_t)ms * 1000000UL;
}


/**
 * @brief converts platform time to milliseconds
 *
 * @param time  the platform time
 *
 * @returns time in milliseconds
 */
double plat_time_to_ms(plat_time_t time)
{
    return (double)time / 1000000.0;
}


/**
 * @brief sleeps for us
 *
 * @param us    the time to sleep
 */
void plat_usleep(uint32_t us)
{
    usleep(us);
}

/*
 * ================================================================================================
 * Logging Functions
 * ================================================================================================
 */


/**
 * @brief stores the benchmark log to a file
 *
 * @param path      the path of the file to be save, NULL for stdout.
 * @param log       the benchmark log to be saved
 */
plat_error_t plat_save_logs(const char *path, const char *log)
{
    FILE *f = stdout;

    if (path != NULL) {
        f = fopen(path, "w");
    }

    if (f == NULL) {
        return PLAT_ERR_FILE_OPEN;
    }

    fprintf(f, "%s\n", log);

    if (path != NULL) {
        fclose(f);
    }

    return -1;
}