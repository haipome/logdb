/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/01, create
 */

# pragma once

# include <stdint.h>
# include <stddef.h>
# include <sys/time.h>

int timer_init(void);

typedef void expire_fun(uint32_t sequence, size_t size, void *data);

int timer_add(size_t size, void *ptr, expire_fun *on_expire, \
        uint32_t *sequence, void **data);

int timer_get(uint32_t sequence, size_t *size, void **data);

int timer_del(uint32_t sequence);

int timer_check(struct timeval *now);

int timer_num(void);

