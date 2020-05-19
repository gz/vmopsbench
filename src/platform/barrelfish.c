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
#include <time.h>
#include <numa.h>
#include <pthread.h>

#include <barrelfish/barrelfish.h>
#include <barrelfish/spawn_client.h>
#include <skb/skb.h>
#include <bench/bench.h>

#include "platform.h"
#include "../logging.h"
#include "../benchmarks/benchmarks.h"

#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)

/*
 * ================================================================================================
 * CNODE for Memory Objects
 * ================================================================================================
 */


struct capref memobj_cnodecap;
struct cnoderef memobj_cnoderef;

/*
 * ================================================================================================
 * Platform Initialization
 * ================================================================================================
 */


static struct thread_sem init_sem = THREAD_SEM_INITIALIZER
;

static int remote_init(void *dumm)
{
//    errval_t err = rsrc_join(my_rsrc_id);
    //assert(err_is_ok(err));


//    debug_printf("remote_init %d\n", disp_get_core_id());
    thread_sem_post(&init_sem);
    thread_detach(thread_self());
    return 0;
}

static int cores_initialized = 1;

static void domain_init_done(void *arg,
                             errval_t err)
{
  //  debug_printf("domain_init_done %d\n", disp_get_core_id());
    assert(err_is_ok(err));
    cores_initialized++;
}


/**
 * @brief initializes the platform backend
 *
 * @param cfg  the benchmark configuration
 *
 * @returns error value
 */
plat_error_t plat_init(struct vmops_bench_cfg *cfg)
{
    errval_t err;

    LOG_PRINT("Initializing VMOPS bench on Barrelfish\n");

    bench_init();

    uint32_t eax, ebx;
    cpuid(0x40000010, &eax, &ebx, NULL, NULL);

    LOG_PRINT("Virtual TSC frequency: %d kHz\n", eax);
    LOG_PRINT("Bench TSC frequency: %" PRIu64 "kHz\n", bench_tsc_per_us() * 1000);
    LOG_PRINT("Bench TSC per ms: %" PRIu64 "\n", bench_tsc_per_ms());


    err = skb_client_connect();
    if (err_is_fail(err)) {
        return PLAT_ERR_INIT_FAILED;
    }

    LOG_INFO("creating the span cnode\n");

    cslot_t retslots;
    struct capref spancn = { .cnode = cnode_root, .slot = ROOTCN_SLOT_SPAN_CN };
    err = cnode_create_raw(spancn, &memobj_cnoderef, ObjType_L2CNode, L2_CNODE_SLOTS, &retslots);
    if (err_is_fail(err)) {
        debug_printf("failed to create cnode\n");
        return err_push(err, LIB_ERR_CNODE_CREATE);
    }



    /* Span domain to all cores */
    for (int i = 0; i <  cfg->corelist_size; ++i) {
        //for (int i = my_core_id + BOMP_DEFAULT_CORE_STRIDE; i < nos_threads + my_core_id; i++) {
        coreid_t core = cfg->coreslist[i];
        err = domain_new_dispatcher(core, domain_init_done, NULL);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "failed to span domain");
            printf("Failed to span domain to %d\n", i);
            assert(err_is_ok(err));
        }
    }

    while (cores_initialized <  cfg->corelist_size) {
        thread_yield();
    }

    /* Run a remote init function on remote cores */
    for (int i = 0; i <  cfg->corelist_size; i++) {
        coreid_t core = cfg->coreslist[i];
        err = domain_thread_create_on(core, remote_init, NULL, NULL);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "domain_thread_create_on failed");
            printf("domain_thread_create_on failed on %d\n", i);
            assert(err_is_ok(err));
        }
        thread_sem_wait(&init_sem);
    }

    LOG_INFO("SPANNED TO ALL!\n");


    // for (uint32_t i = 0; i < cfg->corelist_size; i++) {
    //     LOG_INFO("spanning domain to core %d\n", cfg->coreslist[i]);
    //     if (disp_get_core_id() == cfg->coreslist[i]) {
    //         continue;
    //     }

    //     err = spawn_span(cfg->coreslist[i]);
    //     if (err_is_fail(err)) {
    //         DEBUG_ERR(err, "failed to span!");
    //         return PLAT_ERR_INIT_FAILED;
    //     }
    // }

    return PLAT_ERR_OK;
}


/*
 * ================================================================================================
 * Platform Topology
 * ================================================================================================
 */


static plat_error_t get_topolocy_no_numa(uint32_t **coreids, uint32_t *ncoreids)
{
    errval_t err;

    /* the the number of cores from the SKB */
    err = skb_execute_query("get_system_topology(Nnodes,Ncores,Lnodes,Lcores,Llocalities),"
                            "writeln(num(nodes(Nnodes),cores(Ncores))),"
                            "writeln(Lnodes),writeln(Lcores), writeln(Llocalities).");
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "skb query failed");
        return PLAT_ERR_INIT_FAILED;
    }

    uint32_t ncid = 0;
    uint32_t node = 0;
    err = skb_read_output("num(nodes(%d), cores(%d))", &node, &ncid);
    if (err_is_fail(err)) {
        return err_push(err, NUMA_ERR_SKB_DATA);
    }

    LOG_WARN("seleciting cores using sequential fallback. nproc = %d\n", ncid);

    uint32_t *cids = malloc(ncid * sizeof(uint32_t));
    if (cids == NULL) {
        return PLAT_ERR_NO_MEM;
    }

    for (uint32_t i = 0; i < ncid; i++) {
        cids[i] = i + 1;
    }

    cids[ncid - 1] = 0;

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

    struct bitmap **nodecpus = malloc(nnodes * sizeof(struct bitmap *));
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

    uint32_t *cids = malloc(nproc * sizeof(uint32_t));
    uint32_t cidx = 0;

    (void)(corepolicy);

    switch (numapolicy) {
    case PLAT_TOPOLOGY_NUMA_FILL:
        LOG_INFO("using NUMA fill policy. Ignoring hyperthread policy.\n");
        for (uint32_t n = 0; n < nnodes; n++) {
            for (uint32_t c = 1; c < nproc; c++) {
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
            for (uint32_t c = 1; c < nproc; c++) {
                if (numa_bitmask_isbitset(nodecpus[n], c)) {
                    cids[n + (cidx++) * nnodes] = c;
                }
            }
        }
        break;
    default:
        return PLAT_ERR_ARGS_INVALID;
    }


    cids[cidx++] = 0;

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
    struct capref frame;       ///< the frame capability
    struct frame_identity id;  ///< information about the frame identity
};

static cslot_t memobj_slot = 0;


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
    errval_t err;
    plat_error_t perr = PLAT_ERR_OK;

    if (memobj == NULL || path == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    struct plat_memobj *plat_mobj = malloc(sizeof(struct plat_memobj));
    if (plat_mobj == NULL) {
        return PLAT_ERR_NO_MEM;
    }


    struct capref frame;
    err = frame_alloc(&frame, size, NULL);
    if (err_is_fail(err)) {
        perr = PLAT_ERR_MEMOBJ_CREATE;
        goto err_out_1;
    }

    err = frame_identify(frame, &plat_mobj->id);
    if (err_is_fail(err)) {
        perr = PLAT_ERR_MEMOBJ_CREATE;
        goto err_out_2;
    }

    plat_mobj->frame.cnode = memobj_cnoderef;
    plat_mobj->frame.slot = memobj_slot++;

    err = cap_copy(plat_mobj->frame, frame);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to copy to the cnode\n");
    }

    cap_destroy(frame);


    *memobj = (plat_memobj_t)plat_mobj;

    return PLAT_ERR_OK;

err_out_2:
    cap_destroy(frame);

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

    if (!capref_is_null(plat_mobj->frame)) {
        cap_destroy(plat_mobj->frame);
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
    errval_t err;

    struct plat_memobj *plat_mobj = (struct plat_memobj *)memobj;

    if (capref_is_null(plat_mobj->frame) || plat_mobj->id.bytes < offset + size) {
        return PLAT_ERR_ARGS_INVALID;
    }

    if (addr == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    vregion_flags_t flags = VREGION_FLAGS_READ_WRITE;
    if (huge) {
        flags |= VREGION_FLAGS_LARGE;
    }

    void *map_addr;
    err = vspace_map_one_frame_attr(&map_addr, size, plat_mobj->frame, flags, NULL, NULL);
    if (err_is_fail(err)) {
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
    errval_t err;

    struct plat_memobj *plat_mobj = (struct plat_memobj *)memobj;
    if (capref_is_null(plat_mobj->frame) || plat_mobj->id.bytes < offset + size) {
        return PLAT_ERR_ARGS_INVALID;
    }

    if (addr == NULL) {
        return PLAT_ERR_ARGS_INVALID;
    }

    vregion_flags_t flags = VREGION_FLAGS_READ_WRITE;
    if (huge) {
        flags |= VREGION_FLAGS_LARGE;
    }

    err = vspace_map_one_frame_one_map_fixed_attr((lvaddr_t)addr, size, plat_mobj->frame, flags,
                                                  NULL, NULL);
    if (err_is_fail(err)) {
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
    errval_t err;


    vregion_flags_t flags = VREGION_FLAGS_READ;
    if (perms == PLAT_PERM_READ_WRITE) {
        flags = VREGION_FLAGS_READ_WRITE;
    }

    err = vspace_change_flags((lvaddr_t)addr, size, flags);
    if (err_is_fail(err)) {
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
    errval_t err;

    err = vspace_unmap(addr);
    if (err_is_fail(err)) {
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
    struct thread *thread;
    uint32_t coreid;
    plat_thread_fn_t run;
    struct vmops_bench_run_arg *st;
};


static errval_t send_memobj_cap(coreid_t coreid, plat_memobj_t mobj)
{
    errval_t err;

    struct plat_memobj *plat_mobj = (struct plat_memobj *)mobj;

    LOG_INFO("trying to send the capability to the memobj to core %d...\n", coreid);

    err = domain_send_cap(coreid, plat_mobj->frame);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to obtain the cap\n");
        return err;
    }

    return SYS_ERR_OK;
}


static int plat_thread_run_fn(void *st)
{
    errval_t err;

    struct plat_thread *thr = (struct plat_thread *)st;

    struct plat_memobj *plat_mobj = (struct plat_memobj *)thr->st->memobj;


    struct frame_identity id;
    err = frame_identify(plat_mobj->frame, &id);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to obtain the cap\n");
    }

    if (plat_mobj->id.base != id.base) {
        LOG_ERR("obtained capability is not the same as source!\n");
    }

    thr->run(thr->st);

    return 0;
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
    errval_t err;

    struct plat_thread *thread = malloc(sizeof(struct plat_thread));
    if (thread == NULL) {
        return NULL;
    }

    thread->coreid = coreid;
    thread->run = run;
    thread->st = st;

    if (coreid != disp_get_core_id()) {
        err = send_memobj_cap(coreid, st->memobj);
        if (err_is_fail(err)) {
            LOG_ERR("failed to send the capability to the core\n");
            free(thread);
            return NULL;
        }
    }

    for (size_t i = 0; i < 16; i++) {
        event_dispatch_non_block(get_default_waitset());
        thread_yield();
    }


    LOG_INFO("creating thread on core %u...\n", coreid);
    err = domain_thread_create_on(coreid, plat_thread_run_fn, thread, &thread->thread);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to create the thread\n");
        USER_PANIC_ERR(err, "failed to create the thread\n");
        free(thread);
        return NULL;
    }

    for (size_t i = 0; i < 16; i++) {
        event_dispatch_non_block(get_default_waitset());
        thread_yield();
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
    LOG_WARN("plat_thread_cancel is not implemented!\n");

    struct plat_thread *thr = (struct plat_thread *)thread;

    memset((void *)thr, 0, sizeof(*thr));
    free(thr);

    return PLAT_ERR_OK;
}


typedef struct pthread_barrier {
    unsigned max;
    volatile unsigned cycle;
    volatile unsigned counter;
} pthread_barrier_t;


int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr,
                         unsigned max_count)
{
    barrier->max = max_count;
    barrier->cycle = 0;
    barrier->counter = 0;

    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{
    int cycle = barrier->cycle;
    if (__sync_fetch_and_add(&barrier->counter, 1) == barrier->max - 1) {
        barrier->counter = 0;
        barrier->cycle = !barrier->cycle;
    } else {
        uint64_t waitcnt = 0;

        while (cycle == barrier->cycle) {
            if (waitcnt == 0x400) {
                waitcnt = 0;
                thread_yield();
            }
            waitcnt++;
        }
    }

    return 0;
}


int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    // no dynamically allocated objects to be freed
    return 0;
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
    errval_t err;

    struct plat_thread *plat_thread = (struct plat_thread *)other;

    int retval;
    err = domain_thread_join(plat_thread->thread, &retval);

    memset(plat_thread, 0, sizeof(*plat_thread));
    free(plat_thread);

    if (err_is_fail(err)) {
        return PLAT_ERR_THREAD_JOIN;
    }

    return PLAT_ERR_OK;
}


/*
 * ================================================================================================
 * Timing functions
 * ================================================================================================
 */


/**
 * @brief reads the time time
 *
 * @returns time as a plat_time_t
 */
plat_time_t plat_get_time(void)
{
    return bench_tsc();
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
    return (plat_time_t)ms * bench_tsc_per_ms();
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
    return (double)bench_tsc_to_us(time) / 1000.0;
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