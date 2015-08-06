/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/01, create
 */


# include <stdint.h>
# include <stdlib.h>

# define INITIAL_POOL_SIZE 64

struct cache_t
{
    size_t buf_size;
    size_t free_total;
    size_t free_curr;
    void   **free_ptr;
};

static struct cache_t *const_cache_create(size_t buf_size)
{
    struct cache_t *cache = calloc(1, sizeof(struct cache_t));
    if (cache == NULL)
        return NULL;

    cache->free_ptr = calloc(INITIAL_POOL_SIZE, sizeof(void *));
    if (cache->free_ptr == NULL)
        return NULL;

    cache->free_total = INITIAL_POOL_SIZE;
    cache->buf_size = buf_size;

    return cache;
}

static void *const_cache_alloc(struct cache_t *cache)
{
    if (cache->free_curr > 0)
        return cache->free_ptr[--cache->free_curr];

    return malloc(cache->buf_size);
}

static void const_cache_free(struct cache_t *cache, void *obj)
{
    if (cache->free_curr < cache->free_total)
    {
        cache->free_ptr[cache->free_curr++] = obj;
    }
    else
    {
        size_t new_total = cache->free_total * 2;
        void **new_free = realloc(cache->free_ptr, new_total * sizeof(void *));
        if (new_free != NULL)
        {
            cache->free_total = new_total;
            cache->free_ptr = new_free;
            cache->free_ptr[cache->free_curr++] = obj;
        }
        else
        {
            free(obj);
        }
    }
}

static struct cache_t *g_caches[14];

int cache_init(void)
{
    int i;
    for (i = 0; i < 14; ++i)
    {
        if ((g_caches[i] = const_cache_create(1 << (i + 3))) == NULL)
            return -__LINE__;
    }

    return 0;
}

static inline struct cache_t *cache_choice(size_t size)
{
    int i;
    for (i = 0; i < 14; ++i)
    {
        if (size < ((size_t)1 << (i + 3)))
            return g_caches[i];
    }

    return NULL;
}

void *cache_alloc(size_t size)
{
    struct cache_t *cache = cache_choice(size);
    if (cache == NULL)
        return malloc(size);

    return const_cache_alloc(cache);
}

void cache_free(void *ptr, size_t size)
{
    struct cache_t *cache = cache_choice(size);
    if (cache == NULL)
        return free(ptr);

    const_cache_free(cache, ptr);
}

