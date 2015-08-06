/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/02, create
 */

# include "protocol.h"
# include "serialize.h"

int get_head(struct protocol_head *head, void **p, int *left)
{
    NEG_RET_LN(get_uint8(p, left, &head->result));
    NEG_RET_LN(get_uint8(p, left, &head->command));
    NEG_RET_LN(get_uint32(p, left, &head->sequence));
    
    static void   *echo_buf;
    static size_t echo_buf_len;
    int echo_len = 0;
    NEG_RET_LN(echo_len = get_bin2(p, left, &echo_buf, &echo_buf_len));
    head->echo_len = (uint16_t)echo_len;
    head->echo = echo_buf;

    return 0;
}

int add_head(struct protocol_head *head, void **p, int *left)
{
    NEG_RET_LN(add_uint8(p, left, head->result));
    NEG_RET_LN(add_uint8(p, left, head->command));
    NEG_RET_LN(add_uint32(p, left, head->sequence));
    NEG_RET_LN(add_bin2(p, left, head->echo, head->echo_len));

    return 0;
}

