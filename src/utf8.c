/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/05/29, create
 */


# include <stdint.h>
# include <stddef.h>

# include "utf8.h"

ucs4_t getu8c(char **src, int *illegal)
{
    static char umap[256] = { 0 };
    static int  umap_init_flag = 0;

    if (umap_init_flag == 0)
    {
        int i;

        for (i = 0; i < 0x100; ++i)
        {
            if (i < 0x80)
            {
                umap[i] = 1;
            }
            else if (i >= 0xc0 && i < 0xe0)
            {
                umap[i] = 2;
            }
            else if (i >= 0xe0 && i < 0xf0)
            {
                umap[i] = 3;
            }
            else if (i >= 0xf0 && i < 0xf8)
            {
                umap[i] = 4;
            }
            else if (i >= 0xf8 && i < 0xfc)
            {
                umap[i] = 5;
            }
            else if (i >= 0xfc && i < 0xfe)
            {
                umap[i] = 6;
            }
            else
            {
                umap[i] = 0;
            }
        }

        umap_init_flag = 1;
    }

    uint8_t *s = (uint8_t *)(*src);
    int r_illegal = 0;

    while (umap[*s] == 0)
    {
        ++s;
        ++r_illegal;
    }

    uint8_t *t;
    int byte_num;
    uint32_t uc;
    int i;

repeat_label:
    t = s;
    byte_num = umap[*s];
    uc = *s++ & (0xff >> byte_num);

    for (i = 1; i < byte_num; ++i)
    {
        if (umap[*s])
        {
            r_illegal += s - t;
            goto repeat_label;
        }
        else
        {
            uc = (uc << 6) + (*s & 0x3f);
            s += 1;
        }
    }

    *src = (char *)s;
    if (illegal)
    {
        *illegal = r_illegal;
    }

    return uc;
}

size_t u8decode(char const *str, ucs4_t *des, size_t n, int *illegal)
{
    if (n == 0)
        return 0;

    char *s = (char *)str;
    size_t i = 0;
    ucs4_t uc = 0;
    int r_illegal_all = 0, r_illegal;

    while ((uc = getu8c(&s, &r_illegal)))
    {
        if (i < (n - 1))
        {
            des[i++] = uc;
            r_illegal_all += r_illegal;
        }
        else
        {
            break;
        }
    }

    des[i] = 0;
    if (illegal)
    {
        *illegal = r_illegal_all + r_illegal;
    }

    return i;
}

# define IF_CAN_HOLD(left, n) do { \
    size_t m = (size_t)(n); \
    if ((size_t)(left) < (m + 1)) return -2; \
    (left) -= m; \
} while (0)

int putu8c(ucs4_t uc, char **des, size_t *left)
{
    if (uc < 0)
        return -1;

    if (uc < (0x1 << 7))
    {
        IF_CAN_HOLD(*left, 1);

        **des = (char)uc;
        *des += 1;
        **des = 0;

        return 1;
    }

    int byte_num;

    if (uc < (0x1 << 11))
    {
        byte_num = 2;
    }
    else if (uc < (0x1 << 16))
    {
        byte_num = 3;
    }
    else if (uc < (0x1 << 21))
    {
        byte_num = 4;
    }
    else if (uc < (0x1 << 26))
    {
        byte_num = 5;
    }
    else
    {
        byte_num = 6;
    }

    IF_CAN_HOLD(*left, byte_num);

    int i;
    for (i = byte_num - 1; i > 0; --i)
    {
        *(uint8_t *)(*des + i) = (uc & 0x3f) | 0x80;
        uc >>= 6;
    }

    *(uint8_t *)(*des) = uc | (0xff << (8 - byte_num));

    *des += byte_num;
    **des = 0;

    return byte_num;
}

size_t u8encode(ucs4_t *us, char *des, size_t n, int *illegal)
{
    if (n == 0)
        return 0;

    char *s = des;
    size_t left = n;
    size_t len = 0;
    int r_illegal = 0;

    *s = 0;
    while (*us)
    {
        int ret = putu8c(*us, &s, &left);
        if (ret > 0)
        {
            len += ret;
        }
        else if (ret == -1)
        {
            r_illegal += 1;
        }
        else
        {
            break;
        }
        
        ++us;
    }

    if (illegal)
    {
        *illegal = r_illegal;
    }

    return len;
}

