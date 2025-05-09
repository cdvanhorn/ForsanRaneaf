/**
 * @file mem_manager.c
 */

#include "../mem_manager.h"
#include "mem_pool.h"
#include "../defines.h"

#include <stdbool.h>
#include <stdlib.h>

static bool mem_pooled = false; //!< are we using pooled memory

/**
 * @brief Initialize memory manager
 * @param pool_cfg pointer to mmgr_pool_cfg if using memory pool for allocation
 * @return int 0 on success 1 on failure
 */
int mmgr_open(const struct mmgr_pool_cfg *pool_cfg)
{
	if (pool_cfg != NULL) { // we're using an underlying memory pool
		mem_pooled = true;
		return mmgr_pool_open(pool_cfg);
	}
	return FUNC_SUCCESS;
}

/**
 * @brief Close the memory manager
 */
void mmgr_close()
{
	if (mem_pooled)
		mmgr_pool_close();
}

/**
 * @brief allocate memory
 * @param size size_t amount of memory in bytes to allocate
 * @return void pointer to allocated memory
 */
void *mmgr_alloc(size_t size)
{
	if (mem_pooled)
		return plalloc(size);
	return malloc(size);
}

/**
 * @brief Free allocated memory
 * @param mem void pointer to memory to be freed
 */
void mmgr_free(void *mem)
{
	if (mem_pooled) {
		plfree(mem);
		return;
	}
	free(mem);
}
