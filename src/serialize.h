/*
 * Description: a simple serialize lib for network programing.
 *     History: damonyang@tencent.com, 2013/06/14, create
 */

# pragma once

# include <stdint.h>

int add_int8(void **buf, int *len, int8_t value);
int add_int16(void **buf, int *len, int16_t value);
int add_int32(void **buf, int *len, int32_t value);
int add_int64(void **buf, int *len, int64_t value);
int add_uint8(void **buf, int *len, uint8_t value);
int add_uint16(void **buf, int *len, uint16_t value);
int add_uint32(void **buf, int *len, uint32_t value);
int add_uint64(void **buf, int *len, uint64_t value);
int add_float(void **buf, int *len, float value);
int add_double(void **buf, int *len, double value);

int get_int8(void **buf, int *len, int8_t *value);
int get_int16(void **buf, int *len, int16_t *value);
int get_int32(void **buf, int *len, int32_t *value);
int get_int64(void **buf, int *len, int64_t *value);
int get_uint8(void **buf, int *len, uint8_t *value);
int get_uint16(void **buf, int *len, uint16_t *value);
int get_uint32(void **buf, int *len, uint32_t *value);
int get_uint64(void **buf, int *len, uint64_t *value);
int get_float(void **buf, int *len, float *value);
int get_double(void **buf, int *len, double *value);

/*
 * NOTE:
 * 
 * The get_str, get_str1, get_str2, get_bin1, get_bin2 is like
 * getline. If *value is NULL, they will allocate a buffer for
 * storing the value, which should be freed by the user program.
 * Alternatively, before calling them, *value can contain a pointer
 * to a malloc()-allocated buffer *n bytes in size. If the buffer
 * is not large enough to hold the value, they will resizes it
 * with realloc(), updating *value and *n as necessary.
 *
 * The return value is the real length of value. The *n is the
 * length of buffer, not the real length of value.
 *
 * You can define the value and n in static to avoid free them
 * every time.
 */

int add_str(void **buf, int *len, char *value);
int add_str1(void **buf, int *len, char *value);
int add_str2(void **buf, int *len, char *value);

int get_str(void **buf, int *len, char **value, size_t *n);
int get_str1(void **buf, int *len, char **value, size_t *n);
int get_str2(void **buf, int *len, char **value, size_t *n);

int add_bin(void **buf, int *len, void *value, size_t n);
int add_bin1(void **buf, int *len, void *value, size_t n);
int add_bin2(void **buf, int *len, void *value, size_t n);

int get_bin(void **buf, int *len, void *value, size_t n);
int get_bin1(void **buf, int *len, void **value, size_t *n);
int get_bin2(void **buf, int *len, void **value, size_t *n);

# undef  NEG_RET_LN
# define NEG_RET_LN(x) do { \
    if ((x) < 0) { \
        return -__LINE__; \
    } \
} while (0)

