/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/02/24, create
 */

# pragma once

# include <stdint.h>

int sequence_init(void);

uint64_t sequence_get(void);

void sequence_fini(void);

