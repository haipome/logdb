/*
 * Description: bucket hash table, base on share memory.
 *     History: damonyang@tencent.com, 2013/05/02, create
 */

# pragma once

# include <stdio.h>
# include <stdint.h>
# include <stddef.h>
# include <time.h>
# include <sys/types.h>

/* 比较函数，相等返回 0, 不想等返回非 0 值 */
typedef int compare_fun_t(const void *a, const void *b);

/* 返回单元的 32 位无符号整数 hash Key */
typedef uint32_t hashkey_fun_t(const void *unit);

/**
 * 淘汰函数
 * 当遍历所有桶都找不到空节点时，如果定义了淘汰函数，
 * 再次遍历所有桶，淘汰返回值大于 0 且最大的一个单元
 */
typedef int eliminate_fun_t(void *unit, time_t now);

# define BHASH_MAX_ROW 100

typedef struct
{
    void * mem;                     /* 内存指针 */
    size_t mem_size;                /* 内存大小 */
    size_t unit_size;               /* 单元大小 */
    size_t unit_num;                /* 内存中单元数 */
    size_t row;                     /* 桶深 */
    size_t mods[BHASH_MAX_ROW];
    void * zero;

    compare_fun_t   *compare;
    hashkey_fun_t   *hashkey;
    eliminate_fun_t *eliminate;
} bhash_t;

/*
 * 初始化 bhash
 *   unit_num:  hash 的单元数，实际获取的单元数会比此略大
 *   unit_size: 每个单元大小
 *   row:       桶深，最大为 BHASH_MAX_ROW
 *              桶深越深，内存利用率越高，但速度越慢
 *   shm_key:   共享内存 key
 *              如果为 0 则调用 calloc 获取内存
 *   compare:   单元的比较函数，相等返回 0
 *              如果为 NULL 则取单元前 4 字节作为整数进行比较
 *   hashkey:   返回单元的 hash key
 *              如果为 NULL 则取单元前 4 字节为 hash key
 *   eliminate: 淘汰函数，如果不为 NULL, 当插入没有找到空节点时，
 *              遍历所有桶，淘汰返回值大于 0 且最大的一个单元
 *
 * 返回值：返回 0 表示成功，返回负数表示失败
 *
 * NOTE: 当使用共享内存时，如果 unit_num 或 unit_size 改变，
 *       则需先删除共享内存，否则可能会出错
 */
int bhash_init(bhash_t *hash, size_t unit_num, size_t unit_size, size_t row,
        key_t shm_key, compare_fun_t *compare, hashkey_fun_t *hashkey,
        eliminate_fun_t *eliminate);

/*
 * 在 hash 中查找和 unit 相等的单元
 * 如果成功返回单元指针，否则返回 NULL
 */
void *bhash_get(bhash_t *hash, const void *unit);

/*
 * 把 unit 放到 hash 中的一个空节点
 * 成功返回 hash 中该单元指针，否则返回 NULL
 */
void *bhash_put(bhash_t *hash, const void *unit);

/*
 * 首先 get，找不到则 put
 * 如果 exist 不为 NULL，且 unit 为新添加，则置 exist 为 0
 * 如果 exist 不为 NULL，且 unit 已存在，则置  exist 为 1
 */
void *bhash_add(bhash_t *hash, const void *unit, int *exist);

/*
 * 删除 hash 中和 unit 相等的第一个单元
 * 成功返回 0, 返回负数表示失败
 *
 * NOTE: 也可以通过直接把单元清零进行删除
 */
int bhash_del(bhash_t *hash, const void *unit);

/*
 * 遍历函数，unit 为数据单元，arg 为用户自定义参数
 * 返回 0 表示成功
 */
typedef int traval_fun_t(bhash_t *hash, void *unit, void *arg);

/*
 * 遍历 hash 中所有非空节点
 * arg 会传给 traval 函数，可以为 NULL
 * 返回 traval 返回成功的数量
 */
size_t bhash_traval(bhash_t *hash, traval_fun_t *traval, void *arg);

/* 返回 hash 中单元的使用数 */
size_t bhash_use(bhash_t *hash);

