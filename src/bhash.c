/*
 * Description: bucket hash table, base on share memory.
 *     History: damonyang@tencent.com, 2013/05/02, create
 */

# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdbool.h>
# include <string.h>
# include <strings.h>
# include <time.h>

# include "bhash.h"
# include "utils.h"

static bool is_prime(size_t x)
{
    if (x <= 1)
        return false;

    size_t i;

    for (i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
            return false;
    }

    return true;
}

static int get_primes(size_t base, size_t *primes, int n)
{
    size_t p = base;
    int i;

    for (i = 0; i < n; ++i)
    {
        while (!is_prime(p))
            ++p;

        primes[i] = p;
        ++p;
    }

    return 0;
}

static int default_compare_fun(const void *a, const void *b)
{
    return *((uint32_t *)a) != *((uint32_t *)b);
}

static uint32_t default_hashkey_fun(const void *unit)
{
    return *((uint32_t *)unit);
}

static size_t cal_unit_num(bhash_t *hash)
{
    size_t i;
    size_t sum = 0;

    for (i = 0; i < hash->row; ++i)
    {
        sum += hash->mods[i];
    }

    return sum;
}

int bhash_init(bhash_t *hash, size_t unit_num, size_t unit_size, size_t row,
        key_t shm_key, compare_fun_t *compare, hashkey_fun_t *hashkey,
        eliminate_fun_t *eliminate)
{
    if (!hash || !unit_size || !unit_num)
        return -10;

    if (row > BHASH_MAX_ROW)
        row = BHASH_MAX_ROW;

    memset(hash, 0, sizeof(bhash_t));

    hash->row       = row;
    get_primes((unit_num + row - 1) / row, hash->mods, row);
    hash->unit_size = unit_size;
    hash->unit_num  = cal_unit_num(hash);

    /* use the last as zero */
    hash->mem_size  = (hash->unit_num + 1) * unit_size;

    if (shm_key)
    {
        if (get_shm(shm_key, hash->mem_size, &hash->mem) < 0)
            return -2;
    }
    else
    {
        if ((hash->mem = calloc(1, hash->mem_size)) == NULL)
            return -3;
    }

    hash->zero = hash->mem + hash->unit_size * hash->unit_num;
    bzero(hash->zero, hash->unit_size);

    if (compare == NULL)
        hash->compare = default_compare_fun;
    else
        hash->compare = compare;

    if (hashkey == NULL)
        hash->hashkey = default_hashkey_fun;
    else
        hash->hashkey = hashkey;

    hash->eliminate = eliminate;

    return 0;
}

static inline void *__find__(bhash_t *hash, uint32_t key, const void *key_unit)
{
    size_t i;
    void *table = hash->mem, *node;

    for (i = 0; i < hash->row; ++i)
    {
        node = table + (key % hash->mods[i]) * hash->unit_size;

        if (hash->compare(node, key_unit) == 0)
            return node;

        table += hash->mods[i] * hash->unit_size;
    }

    return NULL;
}

void *bhash_get(bhash_t *hash, const void *unit)
{
    if (!hash || !unit)
        return NULL;

    return __find__(hash, hash->hashkey(unit), unit);
}

static inline void *__eliminate__(bhash_t *hash, uint32_t key)
{
    void *table = hash->mem, *node, *target = NULL;
    time_t now = time(NULL);
    int max = 0;
    size_t i;

    for (i = 0; i < hash->row; ++i)
    {
        node = table + (key % hash->mods[i]) * hash->unit_size;

        int ret = hash->eliminate(node, now);
        if (max < ret)
        {
            target = node;
            max    = ret;
        }

        table += hash->mods[i] * hash->unit_size;
    }

    return target;
}

void *bhash_put(bhash_t *hash, const void *unit)
{
    if (!hash || !unit)
        return NULL;

    void *node;
    uint32_t key = hash->hashkey(unit);

    node = __find__(hash, key, hash->zero);
    if (node)
    {
        memcpy(node, unit, hash->unit_size);

        return node;
    }

    if (hash->eliminate)
    {
        node = __eliminate__(hash, key);
        if (node)
        {
            memcpy(node, unit, hash->unit_size);

            return node;
        }
    }

    return NULL;
}

void *bhash_add(bhash_t *hash, const void *unit, int *exist)
{
    if (!hash || !unit)
        return NULL;

    void *data = bhash_get(hash, unit);
    if (data)
    {
        if (exist)
            *exist = true;

        return data;
    }

    if (exist)
        *exist = false;

    return bhash_put(hash, unit);
}

int bhash_del(bhash_t *hash, const void *unit)
{
    if (!hash || !unit)
        return -10;

    void *data = bhash_get(hash, unit);
    if (data)
    {
        memset(data, 0, hash->unit_size);

        return 0;
    }

    return -1;
}

size_t bhash_traval(bhash_t *hash, traval_fun_t *traval, void *arg)
{
    if (!hash || !traval)
        return -10;

    size_t success_num = 0;
    size_t i;

    for (i = 0; i < hash->unit_num; ++i)
    {
        void *node = hash->mem + i * hash->unit_size;

        if (hash->compare(node, hash->zero) != 0)
        {
            if (traval(hash, node, arg) == 0)
                ++success_num;
        }
    }

    return success_num;
}

static int empty(bhash_t *hash, void *unit, void *arg)
{
    return 0;
}

size_t bhash_use(bhash_t *hash)
{
    if (!hash)
        return 0;

    return bhash_traval(hash, empty, NULL);
}

