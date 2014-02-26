/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/01, create
 */

# pragma once

# include <stddef.h>

int cache_init(void);

void *cache_alloc(size_t size);

void cache_free(void *ptr, size_t size);

