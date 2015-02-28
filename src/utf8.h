/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/05/29, create
 */

# pragma once

# include <stdint.h>
# include <stddef.h>

typedef int32_t ucs4_t;

ucs4_t getu8c(char **src, int *illegal);
size_t u8decode(char const *str, ucs4_t *des, size_t n, int *illegal);

int putu8c(ucs4_t uc, char **des, size_t *left);
size_t u8encode(ucs4_t *us, char *des, size_t n, int *illegal);

