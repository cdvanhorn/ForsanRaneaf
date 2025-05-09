/**
 * @file mem_manager.h
 */

#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H
#include <stddef.h>
#include <stdint.h>

/**
 * @struct mmgr_pool_cfg
 * @brief object for pool configuration
 */
struct mmgr_pool_cfg {
	uint16_t num_pools; //!< number of pools to create
	uint32_t *chunk_sizes; //!< size of chunk in each pool
	uint32_t *chunk_counts; //!< how many chunks in each pool
};

extern void *mmgr_alloc(size_t size);
extern void mmgr_free(void *mem);
extern int mmgr_open(const struct mmgr_pool_cfg *pool_cfg);
extern void mmgr_close();

#endif //MEM_MANAGER_H
