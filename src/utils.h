/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/17, create
 */


# pragma once

# include <stdio.h>
# include <stdlib.h>
# include <stdbool.h>
# include <error.h>
# include <errno.h>
# include <netinet/in.h>

char *sstrncpy(char *dest, const char *src, size_t n);
size_t lstrncpy(char *dest, const char *src, size_t n);

void *auto_realloc(void **p, size_t *n, size_t size);

char *strtolower(char *str);
char *strtoupper(char *str);

int is_server_exist(const char *name);

char *get_curr_date_time(void);

char *hex_dump_str(const char *data, size_t size);
char *addrtostr(const struct sockaddr_in *addr);
size_t hex_str(char *to, const char *data, size_t len);

char *get_hour_str(int offset, time_t *timeptr);
char *get_day_str (int offset, time_t *timeptr);
char *get_mon_str (int offset, time_t *timeptr);
char *get_year_str(int offset, time_t *timeptr);
char *get_date_str(int offset, time_t *timeptr);
char *get_time_str(int offset, time_t *timeptr);
char *get_datetime_str(int offset, time_t *timeptr);

char *basepath(const char *path);
char *parentpath(char *path);

uint64_t timeval_diff(struct timeval *old, struct timeval *new);
struct timeval *timeval_add(struct timeval *t, time_t usec);

int get_shm(key_t key, size_t size, void **addr);

ssize_t xwrite(int fd, const void *buf, size_t len);
ssize_t write_in_full(int fd, const void *buf, size_t count);

char *raw_input(const char *msg);
bool ask_is_continue(void);

size_t buf_sum(void const *p, size_t n);

# define NEG_RET_LN(x) do { \
    if ((x) < 0) { \
        return -__LINE__; \
    } \
} while (0)

# define NEG_RET(x) do { \
    int __ret = (x); \
    if (__ret < 0) { \
        return __ret; \
    } \
} while (0)

# define NEG_DIE(x) do { \
    if ((x) < 0) { \
        die(); \
    } \
} while (0)

