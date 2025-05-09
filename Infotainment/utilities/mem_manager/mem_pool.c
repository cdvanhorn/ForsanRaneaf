/**
 * @file mem_pool.c
*/

#include "../defines.h"
#include "../logger.h"
#include "../mem_manager.h"
#include "mem_pool.h"

#include <stdlib.h>
#include <string.h>

/**
 * @struct mmgr_chunk_hdr
 * @brief individual chunk in memory pool
 */
struct mmgr_chunk_hdr {
	uint32_t size; //!< size of the data pointer in chunk
	uint32_t used; //!< size of object contained
	struct mmgr_chunk_hdr *next; //!< pointer to next chunk
	struct mmgr_pool *pool; //!< pointer to pool this chunk belongs to
};

/**
 * @struct mmgr_pool
 * @brief memory pool
 */
struct mmgr_pool {
	uint32_t chunk_size; //!< chunk size for this pool
	uint32_t num_chunks; //!< how many chunks this pool contains
	uint32_t num_used_chunks; //!< how many chunks are in use
	struct mmgr_chunk_hdr *chunks; //!< pointer to first chunk
	struct mmgr_chunk_hdr *free; //!< pointer to first free chunk
	struct mmgr_pool *next; //!< pointer to next pool
};

/**
 * @struct mmgr_pool_complex
 * @brief container for all of the memory pools
 */
struct mmgr_pool_complex {
	uint32_t num_pools; //!< how many pools are there
	struct mmgr_pool *pools; //!< pointer to first pool
};

static struct mmgr_pool_complex *pools = NULL; //!< pointer to the pools

static const uint32_t chunk_hdr_size =
	sizeof(struct mmgr_chunk_hdr); //!< size of the chunk header

/**
 * @brief Create a new pool, allocate and initialize all of the chunks in the pool
 * @param chunk_size uint32_t - how big is each chunk of memory in the pool
 * @param num_chunks uint32_t - how man chunks are in the pool
 * @return pointer to mmgr_pool struct of the created pool
 */
static struct mmgr_pool *new_pool(const uint32_t chunk_size,
				  const uint32_t num_chunks)
{
	if (chunk_size == 0 || num_chunks == 0)
		return NULL;

	struct mmgr_pool *pool = malloc(sizeof(struct mmgr_pool));
	if (pool == NULL)
		return NULL;

	pool->chunk_size = chunk_size;
	pool->num_chunks = num_chunks;
	pool->num_used_chunks = 0;

	// allocate and setup chunks
	const uint32_t chunk_step = chunk_size + chunk_hdr_size;
	char *chunks = calloc(num_chunks, chunk_step);
	if (chunks == NULL) {
		free(pool);
		return NULL;
	}
	for (int i = 0; i < num_chunks; i++) {
		struct mmgr_chunk_hdr *chunk = (struct mmgr_chunk_hdr *)chunks;
		chunk = chunk + (i * chunk_step);
		chunk->size = chunk_size;
		chunk->used = 0;
		chunk->pool = pool;
		chunk->next = NULL;

		// not the last chunk
		if (i + 1 != num_chunks)
			chunk->next = chunk + chunk_step;
	}
	pool->chunks = (struct mmgr_chunk_hdr *)chunks;
	pool->free = (struct mmgr_chunk_hdr *)chunks;

	return pool;
}

/**
 * @brief Create and initialize memory pools based on given configuration
 * @param config pointer to mmgr_pool_cfg that contains pool configuration
 * @return int 0 on success 1 on failure
 */
int mmgr_pool_open(const struct mmgr_pool_cfg *config)
{
	if (config == NULL)
		goto mmgr_pool_init_failure;

	if (config->num_pools < 1)
		return FUNC_SUCCESS;

	pools = malloc(sizeof(struct mmgr_pool_complex));
	if (pools == NULL)
		goto mmgr_pool_init_failure;
	pools->num_pools = config->num_pools;
	pools->pools = NULL;

	for (int i = 0; i < config->num_pools; i++) {
		struct mmgr_pool *pool = new_pool(config->chunk_sizes[i],
						  config->chunk_counts[i]);
		if (pool == NULL)
			goto mmgr_pool_init_failure;

		pool->next = pools->pools;
		pools->pools = pool;
	}

	return FUNC_SUCCESS;

mmgr_pool_init_failure:
	mmgr_pool_close();
	log_write(LOG_TAG_ERR, "failed to initialize memory pools");
	return FUNC_FAILURE;
}

/**
 * @brief Free all allocated memory associated with a memory pool.
 * @param pool pointer to mmgr_pool to free memory of
 */
void free_pools(struct mmgr_pool *pool)
{
	while (pool != NULL) {
		free(pool->chunks);
		struct mmgr_pool *npool = pool->next;
		free(pool);
		pool = npool;
	}
}

/**
 * @brief Free all allocated memory associated with memory pools.
 */
void mmgr_pool_close()
{
	if (pools != NULL) {
		free_pools(pools->pools);
		pools->num_pools = 0;
		free(pools);
		pools = NULL;
	}
}

/**
 * @brief Get the raw allocated memory of the next available chunk in the
 *	memory pool
 * @param pool pointer to mmgr_pool - pool to find the next chunk in
 * @param size size_t - amount of memory to allocate
 * @return void pointer to raw memory for use, NULL if memory pool full
 */
void *get_next_chunk(struct mmgr_pool *pool, size_t size)
{
	if (pool->free == NULL) // pool is full
		return NULL;

	struct mmgr_chunk_hdr *chunk = pool->free;
	pool->num_used_chunks += 1;
	pool->free = chunk->next;
	chunk->used = size;
	return chunk + chunk_hdr_size;
}

/**
 * @brief Allocate memory from the memory pool, will attempt to get memory from
 *	the pool with the smallest adequet chunk size
 * @param size size_t - amount of memory to allocate
 * @return void pointer to raw memory for use, NULL if pool full or pools not
 *	setup
 */
void *plalloc(const size_t size)
{
	// find the pool, pools are assumed to be in ascending order by size
	if (pools == NULL || pools->pools == NULL)
		return NULL;

	struct mmgr_pool *pool = pools->pools;
	while (pool != NULL) {
		if (pool->chunk_size >= size) {
			return get_next_chunk(pool, size);
		}
		pool = pool->next;
	}

	return NULL;
}

/**
 * @brief Return the assocated chunk of memory back to the pool
 * @param mem void pointer to chunk of memory to return to pool
 */
void plfree(void *mem)
{
	if (mem == NULL)
		return;
	char *chunk_start = mem;
	struct mmgr_chunk_hdr *chunk =
		(struct mmgr_chunk_hdr *)chunk_start - chunk_hdr_size;
	chunk->used = 0;
	memset(chunk + chunk_hdr_size, 0, sizeof(chunk->size));
	chunk->pool->num_used_chunks -= 1;
	chunk->next = chunk->pool->free;
	chunk->pool->free = chunk;
}
