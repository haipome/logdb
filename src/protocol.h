/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/02, create
 */

# pragma once

# include <stdint.h>
# include <stddef.h>

enum
{
    RESULT_OK,
    RESULT_PKG_FMT_ERROR,
    RESULT_INTERNAL_ERROR,
};

enum
{
    COMMAND_LOG,
    COMMAND_SQL,
};

struct protocol_head
{
    uint8_t  result;
    uint8_t  command;
    uint32_t sequence;
    size_t   echo_len;
    void     *echo;
};

int get_head(struct protocol_head *head, void **p, int *left);

int add_head(struct protocol_head *head, void **p, int *left);

# pragma pack (1)
struct inner_addr
{
    uint32_t ip;
    uint16_t port;
};
# pragma pack()

