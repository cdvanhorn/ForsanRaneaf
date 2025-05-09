/**
* @file mem_pool.h
*/

#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <stddef.h>

extern int mmgr_pool_open(const struct mmgr_pool_cfg *config);
extern void mmgr_pool_close();
extern void *plalloc(size_t size);
extern void plfree(void *mem);

#endif //MEM_POOL_H
