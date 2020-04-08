/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */

#ifndef __VMOPS_PLATFORM_H_
#define __VMOPS_PLATFORM_H_ 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h> /* For off_t constants */


#define PLAT_ARCH_BASE_PAGE_SIZE (1 << 12)
#define PLAT_ARCH_HUGE_PAGE_SIZE (1 << 21)

///< forward declaration
struct vmops_bench_run_arg;
struct vmops_bench_cfg;

/*
 * ================================================================================================
 * Platform Errors
 * ================================================================================================
 */

typedef enum {
    PLAT_ERR_OK = 0,
    PLAT_ERR_ARGS_INVALID,
    PLAT_ERR_INIT_FAILED,
    PLAT_ERR_NO_MEM,
    PLAT_ERR_MEMOBJ_CREATE,
    PLAT_ERR_MEMOBJ_SIZE,
    PLAT_ERR_MAP_FAILED,
    PLAT_ERR_PROTECT_FAILED,
    PLAT_ERR_UNMAP_FAILED,
    PLAT_ERR_THREAD_CREATE,
    PLAT_ERR_THREAD_JOIN,
    PLAT_ERR_FILE_OPEN,
    PLAT_ERR_BARRIER,
} plat_error_t;


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
plat_error_t plat_init(struct vmops_bench_cfg *cfg);


/*
 * ================================================================================================
 * Platform Topology
 * ================================================================================================
 */


typedef enum {
    PLAT_TOPOLOGY_NUMA_FILL,
    PLAT_TOPOLOGY_NUMA_INTERLEAVE,
} plat_topo_numa_t;


typedef enum {
    PLAT_TOPOLOGY_CORES_FILL,
    PLAT_TOPOLOGY_CORES_INTERLEAVE,
} plat_topo_cores_t;


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
                               uint32_t **coreids, uint32_t *ncoreids);


/*
 * ================================================================================================
 * Virtual Memory Operations
 * ================================================================================================
 */


///< opaque file descriptor / pointer to a memory object
typedef void *plat_memobj_t;


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
plat_error_t plat_vm_create(const char *path, plat_memobj_t *memobj, size_t size, bool huge);


/**
 * @brief destroys a created memory object
 *
 * @param memobj    the memory object to be destroyed
 *
 * @returns
 */
plat_error_t plat_vm_destroy(plat_memobj_t memobj);


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
plat_error_t plat_vm_map(void **addr, size_t size, plat_memobj_t memobj, off_t offset, bool huge);


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
                               bool huge);


/**
 * @brief permissions for the page mappigns
 */
typedef enum {
    PLAT_PERM_READ_ONLY,  ///< read only mapping
    PLAT_PERM_READ_WRITE  ///< read write mapping
} plat_perm_t;


/**
 * @brief changes the permissios of a mapping
 *
 * @param addr      the address to protect
 * @param size      the size of the memory region to protect
 * @param perms     the new permissins to set for the region
 *
 * @returns error value
 */
plat_error_t plat_vm_protect(void *addr, size_t size, plat_perm_t perms);


/**
 * @brief unmaps a previously mapped memory region
 *
 * @param addr      the address to be unmapped
 * @param size      the size of the region to be unmapped
 *
 * @returns error value
 */
plat_error_t plat_vm_unmap(void *addr, size_t size);


/*
 * ================================================================================================
 * Threading Functions
 * ================================================================================================
 */


///< defines a platform thread
typedef void *plat_thread_t;

///< defines a platform barrier
typedef void *plat_barrier_t;

///< the run function type
typedef void *(*plat_thread_fn_t)(struct vmops_bench_run_arg *st);


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
                                uint32_t coreid);


/**
 * @brief initializes a platform barrier
 *
 * @param barrier   the barrier to initialize
 * @param nthreads  the number of threads to expect
 *
 * @returns error value
 */
plat_error_t plat_thread_barrier_init(plat_barrier_t *barrier, uint32_t nthreads);


/**
 * @brief destroys an initalized platform barrier
 *
 * @param barrier   the barrier to be destroyed
 *
 * @returns error value
 */
plat_error_t plat_thread_barrier_destroy(plat_barrier_t barrier);


/**
 * @brief calls a barrier to ensure synchronization among the threads
 *
 * @param barrier   the barrier to enter
 *
 * @returns error value
 */
plat_error_t plat_thread_barrier(plat_barrier_t barrier);


/**
 * @brief waits another thread to join
 *
 * @param other     the other thread to be joined
 *
 * @returns return value of the other therad
 */
plat_error_t plat_thread_join(plat_thread_t other);


/**
 * @brief cancels and frees up a already created thread
 *
 * @param thread    the thread to be cancelled
 *
 * @returns error value
 */
plat_error_t plat_thread_cancel(plat_thread_t thread);


/*
 * ================================================================================================
 * Timing functions
 * ================================================================================================
 */


///< defines a time value
typedef uint64_t plat_time_t;

/**
 * @brief reads the time time
 *
 * @returns time as a plat_time_t
 */
plat_time_t plat_get_time(void);


/**
 * @brief converts time in milliseconds to
 *
 * @param ms    the time in ms to convert to platform time
 *
 * @returns converted time in plat_time_t
 */
plat_time_t plat_convert_time(uint32_t ms);


/**
 * @brief converts platform time to milliseconds
 *
 * @param time  the platform time
 *
 * @returns time in milliseconds
 */
double plat_time_to_ms(plat_time_t time);


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
plat_error_t plat_save_logs(const char *path, const char *log);


#endif /* __VMOPS_PLATFORM_H_ */