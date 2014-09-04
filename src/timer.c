/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/01, create
 */

# include <string.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdbool.h>
# include <sys/time.h>

# include "utils.h"
# include "list.h"
# include "cache.h"
# include "bhash.h"
# include "timer.h"

# define TIMER_CHECK_INTERVAL   5000
# define TIMER_TIME_OUT         1000

static struct   list_head vec[TIMER_TIME_OUT];
static bhash_t  hash;
static uint64_t time_pos;
static int      node_num;

struct timer_node
{
    uint32_t            sequence;
    struct list_head    list;
    expire_fun          *on_expire;
    size_t              size;
    void                *data;
};

static uint64_t relative_time(struct timeval *t)
{
    return ((t->tv_sec * 1000000ull) + t->tv_usec) / TIMER_CHECK_INTERVAL;
}

static bool timer_init_flag = false;

int timer_init(void)
{
    int i;
    for (i = 0; i < TIMER_TIME_OUT; ++i)
    {
        INIT_LIST_HEAD(&vec[i]);
    }

    NEG_RET(bhash_init(&hash, 20 * 10000, sizeof(struct timer_node), \
                16, 0, NULL, NULL, NULL));

    NEG_RET(cache_init());

    struct timeval now;
    gettimeofday(&now, NULL);
    time_pos = relative_time(&now);

    timer_init_flag = true;

    return 0;
}

int timer_add(size_t size, void *ptr, expire_fun *on_expire, \
        uint32_t *sequence, void **data)
{
    if (timer_init_flag == false)
        return -1;

    static uint32_t inner_sequence = 0;
    ++inner_sequence;
    if (inner_sequence == 0)
        ++inner_sequence;

    struct timer_node node;
    bzero(&node, sizeof(node));
    node.sequence = inner_sequence;

    struct timer_node *nodeptr = bhash_put(&hash, &node);
    if (nodeptr == NULL)
        return -3;

    nodeptr->on_expire = on_expire;
    nodeptr->size = size;
    nodeptr->data = cache_alloc(size);
    if (nodeptr->data == NULL)
        return -2;
    if (ptr)
        memcpy(nodeptr->data, ptr, size);

    INIT_LIST_HEAD(&nodeptr->list);
    list_add_tail(&nodeptr->list, &vec[time_pos % TIMER_TIME_OUT]);

    *sequence = inner_sequence;
    if (data)
        *data = nodeptr->data;

    ++node_num;

    return 0;
}

int timer_get(uint32_t sequence, size_t *size, void **data)
{
    if (timer_init_flag == false)
        return -1;

    struct timer_node node = { .sequence = sequence };
    struct timer_node *nodeptr = bhash_get(&hash, &node);
    if (nodeptr == NULL)
        return -2;

    *size = nodeptr->size;
    *data = nodeptr->data;

    return 0;
}

static void inner_timer_del(struct timer_node *nodeptr)
{
    cache_free(nodeptr->data, nodeptr->size);
    list_del(&nodeptr->list);
    bhash_del(&hash, nodeptr);

    --node_num;

    return;
}

int timer_del(uint32_t sequence)
{
    if (timer_init_flag == false)
        return -1;

    struct timer_node node = { .sequence = sequence };
    struct timer_node *nodeptr = bhash_get(&hash, &node);
    if (nodeptr == NULL)
        return -2;

    inner_timer_del(nodeptr);

    return 0;
}

int timer_check(struct timeval *now)
{
    if (timer_init_flag == false)
        return -1;

    struct timeval _now;
    if (now == NULL)
    {
        gettimeofday(&_now, NULL);
        now = &_now;
    }

    uint64_t time_curr = relative_time(now);
    if (time_curr == time_pos)
        return 0;

    int count = 0;
    while (time_pos <= time_curr)
    {
        time_pos++;

        struct list_head *head = &vec[time_pos % TIMER_TIME_OUT];
        while (head->next != head)
        {
            struct list_head *curr = head->next;
            struct timer_node *nodeptr = list_entry(curr, struct timer_node, list);

            nodeptr->on_expire(nodeptr->sequence, nodeptr->size, nodeptr->data);
            inner_timer_del(nodeptr);

            ++count;
        }
    }

    return count;
}

int timer_num(void)
{
    return node_num;
}

