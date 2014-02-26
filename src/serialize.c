/*
 * Description: a simple serialize lib for network programing.
 *     History: damonyang@tencent.com, 2013/06/14, create
 */


# include <string.h>
# include <stdlib.h>
# include <endian.h>
# include <limits.h>
# include <byteswap.h>

# include "serialize.h"

# undef htobe16
# undef htobe32
# undef htobe64
# undef htobef
# undef htobed
# undef be16toh
# undef be32toh
# undef be64toh
# undef beftoh
# undef bedtoh

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define htobe16(x) bswap_16(x)
#  define htobe32(x) bswap_32(x)
#  define htobe64(x) bswap_64(x)
#  define be16toh(x) bswap_16(x)
#  define be32toh(x) bswap_32(x)
#  define be64toh(x) bswap_64(x)
# else
#  define htobe16(x) (x)
#  define htobe32(x) (x)
#  define htobe64(x) (x)
#  define be16toh(x) (x)
#  define be32toh(x) (x)
#  define be64toh(x) (x)
# endif

# ifndef __FLOAT_WORD_ORDER
# define __FLOAT_WORD_ORDER __BYTE_ORDER
# endif

# if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN

static inline float __bswap_f(float x)
{
    union { float f; uint32_t i; } u_float = { x };
    u_float.i = bswap_32(u_float.i);

    return u_float.f;
}

static inline double __bswap_d(double x)
{
    union { double d; uint64_t i; } u_double = { x };
    u_double.i = bswap_64(u_double.i);

    return u_double.d;
}

#  define htobef(x) __bswap_f(x)
#  define htobed(x) __bswap_d(x)
#  define beftoh(x) __bswap_f(x)
#  define bedtoh(x) __bswap_d(x)
# else
#  define htobef(x) (x)
#  define htobed(x) (x)
#  define beftoh(x) (x)
#  define bedtoh(x) (x)
# endif

# define NOSWAP(x) (x)

# define PASS_LEN(size) do { \
    *buf = (char *)(*buf) + (size); \
    *len = *len - (size); \
} while (0)

# define BASIC_ARG_CHECK(type) do { \
    if (!buf || !(*buf) || !len) { \
        return -1; \
    } \
    if ((size_t)(*len) < sizeof(type)) { \
        return -2; \
    } \
} while (0)

# define ADD_BASIC(type, swap) do { \
    BASIC_ARG_CHECK(type); \
    *((type *)(*buf)) = swap(value); \
    PASS_LEN(sizeof(type)); \
    return 0; \
} while (0)

# define ADD_FUN(tail, type, swap) int \
    add_ ## tail(void **buf, int *len, type value) { \
        ADD_BASIC(type, swap); \
    }

ADD_FUN(int8,   int8_t,   NOSWAP)
ADD_FUN(int16,  int16_t,  htobe16)
ADD_FUN(int32,  int32_t,  htobe32)
ADD_FUN(int64,  int64_t,  htobe64)
ADD_FUN(uint8,  uint8_t,  NOSWAP)
ADD_FUN(uint16, uint16_t, htobe16)
ADD_FUN(uint32, uint32_t, htobe32)
ADD_FUN(uint64, uint64_t, htobe64)
ADD_FUN(float,  float,    htobef)
ADD_FUN(double, double,   htobed)

# define GET_BASIC(type, swap) do { \
    BASIC_ARG_CHECK(type); \
    *value = swap(*((type *)(*buf))); \
    PASS_LEN(sizeof(type)); \
    return 0; \
} while (0)

# define GET_FUN(tail, type, swap) int \
        get_ ## tail(void **buf, int *len, type *value) { \
            GET_BASIC(type, swap); \
        }

GET_FUN(int8,   int8_t,   NOSWAP)
GET_FUN(int16,  int16_t,  be16toh)
GET_FUN(int32,  int32_t,  be32toh)
GET_FUN(int64,  int64_t,  be64toh)
GET_FUN(uint8,  uint8_t,  NOSWAP)
GET_FUN(uint16, uint16_t, be16toh)
GET_FUN(uint32, uint32_t, be32toh)
GET_FUN(uint64, uint64_t, be64toh)
GET_FUN(float,  float,    beftoh)
GET_FUN(double, double,   bedtoh)

# define ADD_STR_ARG_CHECK(offset, max) do { \
    if (!buf || !(*buf) || !len || !value) { \
        return -1; \
    } \
    slen = strlen(value); \
    if (*len >= 0 && (size_t)(*len) < (slen + (offset))) { \
        return -2; \
    } \
    if (max) { \
        if (slen > (max)) { \
            return -3; \
        } \
    } \
} while (0)

int add_str(void **buf, int *len, char *value)
{
    size_t slen = 0;
    ADD_STR_ARG_CHECK(1, 0);
    strcpy((char *)(*buf), value);
    PASS_LEN(slen + 1);

    return 0;
}

int add_str1(void **buf, int *len, char *value)
{
    size_t slen = 0;
    ADD_STR_ARG_CHECK(1, UINT8_MAX);
    add_uint8(buf, len, (uint8_t)slen);
    memcpy(*buf, value, slen);
    PASS_LEN(slen);

    return 0;
}

int add_str2(void **buf, int *len, char *value)
{
    size_t slen = 0;
    ADD_STR_ARG_CHECK(2, UINT16_MAX);
    add_uint16(buf, len, (uint16_t)slen);
    memcpy(*buf, value, slen);
    PASS_LEN(slen);

    return 0;
}

static inline void *alloc_buf(void **buf, size_t *n, size_t size)
{
    if (*buf == NULL || *n < size)
    {
        void *_buf = *buf;
        size_t _n  = *n;

        if (_buf == NULL)
            _n = 1;

        while (_n < size)
            _n *= 2;

        if ((_buf = realloc(_buf, _n)) == NULL)
            return NULL;

        *buf = _buf;
        *n   = _n;
    }

    return *buf;
}

# define GET_STR_ARG_CHECK do { \
    if (!buf || !(*buf) || !len || !value || !n) { \
        return -1; \
    } \
} while (0)

# define GET_STR(size) do { \
    if (*len >= 0 && (size_t)(*len) < (size)) { \
        return -3; \
    } \
    if (alloc_buf((void **)value, n, (size) + 1) == NULL) { \
        return -4; \
    } \
    memcpy(*value, *buf, (size)); \
    *(*value + (size)) = '\0'; \
    PASS_LEN(size); \
    return size; \
} while (0)

int get_str(void **buf, int *len, char **value, size_t *n)
{
    GET_STR_ARG_CHECK;

    size_t slen = strlen((char *)(*buf));
    if (*len >= 0 && (size_t)(*len) < slen + 1)
        return -2;

    if (alloc_buf((void **)value, n, slen) == NULL)
        return -3;

    strcpy(*value, *buf);

    PASS_LEN(slen + 1);

    return slen;
}

int get_str1(void **buf, int *len, char **value, size_t *n)
{
    GET_STR_ARG_CHECK;

    uint8_t slen = 0;
    if (get_uint8(buf, len, &slen) < 0)
        return -2;

    GET_STR(slen);
}

int get_str2(void **buf, int *len, char **value, size_t *n)
{
    GET_STR_ARG_CHECK;

    uint16_t slen = 0;
    if (get_uint16(buf, len, &slen) < 0)
        return -2;

    GET_STR(slen);
}

# define ADD_BIN_ARG_CHECK(offset, max) do { \
    if (!buf || !(*buf) || !len) { \
        return -1; \
    } \
    if (*len >= 0 && (size_t)(*len) < (n + (offset))) { \
        return -2; \
    } \
    if (max) { \
        if (n > max) { \
            return -3; \
        } \
    } \
} while (0)

int add_bin(void **buf, int *len, void *value, size_t n)
{
    ADD_BIN_ARG_CHECK(0, 0);
    if (n)
        memcpy(*buf, value, n);
    PASS_LEN(n);

    return 0;
}

int add_bin1(void **buf, int *len, void *value, size_t n)
{
    ADD_BIN_ARG_CHECK(1, UINT8_MAX);
    add_uint8(buf, len, (uint8_t)n);
    if (n)
        memcpy(*buf, value, n);
    PASS_LEN(n);

    return 0;
}

int add_bin2(void **buf, int *len, void *value, size_t n)
{
    ADD_BIN_ARG_CHECK(2, UINT16_MAX);
    add_uint16(buf, len, (uint16_t)n);
    if (n)
        memcpy(*buf, value, n);
    PASS_LEN(n);

    return 0;
}

# define GET_BIN_ARG_CHECK do { \
    if (!buf || !(*buf) || !len || !value || !n) { \
        return -1; \
    } \
} while (0)

# define GET_BIN(size) do { \
    if (*len >= 0 && (size_t)(*len) < (size)) { \
        return -3; \
    } \
    if (alloc_buf(value, n, (size)) == NULL) { \
        return -4; \
    } \
    memcpy(*value, *buf, (size)); \
    PASS_LEN(size); \
    return size; \
} while (0)

int get_bin(void **buf, int *len, void *value, size_t n)
{
    if (!buf || !(*buf) || !len)
        return -1;

    if (*len >= 0 && (size_t)(*len) < n)
        return -2;
    
    if (n)
        memcpy(value, *buf, n);

    PASS_LEN(n);

    return 0;
}

int get_bin1(void **buf, int *len, void **value, size_t *n)
{
    GET_BIN_ARG_CHECK;

    uint8_t blen = 0;
    if (get_uint8(buf, len, &blen) < 0)
        return -2;

    GET_BIN(blen);
}

int get_bin2(void **buf, int *len, void **value, size_t *n)
{
    GET_BIN_ARG_CHECK;

    uint16_t blen = 0;
    if (get_uint16(buf, len, &blen) < 0)
        return -2;

    GET_BIN(blen);
}

