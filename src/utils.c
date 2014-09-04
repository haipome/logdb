/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/17, create
 */

# undef  _GNU_SOURCE
# define _GNU_SOURCE /* for getline */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdint.h>
# include <limits.h>
# include <ctype.h>
# include <time.h>
# include <unistd.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/file.h>
# include <sys/types.h>
# include <sys/ipc.h>
# include <sys/shm.h>

# include "utils.h"

char *sstrncpy(char *dest, const char *src, size_t n)
{
    if (n == 0)
        return dest;

    dest[0] = 0;

    return strncat(dest, src, n - 1);
}

size_t lstrncpy(char *dest, const char *src, size_t n)
{
    sstrncpy(dest, src, n);

    size_t len = strlen(src);
    if (len >= n)
        len = n - 1;

    return len;
}

void *auto_realloc(void **p, size_t *n, size_t size)
{
    if (*p == NULL || *n < size)
    {
        void  *_p = *p;
        size_t _n = *n;

        if (_p == NULL || _n == 0)
            _n = 1;

        while (_n < size)
            _n *= 2;

        if ((_p = realloc(_p, _n)) == NULL)
            return NULL;

        *p = _p;
        *n = _n;
    }

    return *p;
}

char *strtolower(char *str)
{
    char *s = str;

    while (*s)
    {
        if (*s >= 'A' && *s <= 'Z')
            *s += ('a' - 'A');

        ++s;
    }

    return str;
}

char *strtoupper(char *str)
{
    char *s = str;

    while (*s)
    {
        if (*s >= 'a' && *s <= 'z')
            *s -= ('a' - 'A');

        ++s;
    }

    return str;
}

char *get_curr_date_time(void)
{
    static char data_time[100];

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    snprintf(data_time, sizeof(data_time), "%04d-%02d-%02d %02d:%02d:%02d", \
            tm->tm_year + 1900, tm->tm_mon + 1, \
            tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    return data_time;
}

int is_server_exist(const char *name)
{
    char lock_file[PATH_MAX];
    snprintf(lock_file, sizeof(lock_file), "/tmp/%s.lock", name);

    int fd = open(lock_file, O_CREAT, 400);
    if (fd < 0)
        return -1;

    if (flock(fd, LOCK_EX|LOCK_NB) < 0)
        return -2;

    return 0;
}

char *hex_dump_str(const char *data, size_t size)
{
    if (size > UINT16_MAX)
        size = UINT16_MAX;
    static char str[5 * UINT16_MAX];

    size_t len = 0;
    size_t n = 0;
    size_t i;

    while (n < size)
    {
        len += snprintf(str + len, sizeof(str) - len, "0x%04zx:  ", n);

        size_t left   = size - n;
        size_t num    = left >= 16 ? 16 : left;
        size_t group  = num / 2;
        size_t single = num % 2;

        char const *line = data + n;

        for (i = 0; i < group; ++i)
        {
            uint8_t *curr = (uint8_t *)&data[n];
            len += snprintf(str + len, sizeof(str) - len, \
                    "%02x%02x ", curr[0], curr[1]);
            n += 2;
        }

        if (group != 8)
        {
            if (single)
            {
                len += snprintf(str + len, sizeof(str) - len, \
                        "%02x   ", (uint8_t)data[n++]);
            }

            size_t blank = 8 - group - single;
            for (i = 0; i < blank; ++i)
                len += snprintf(str + len, sizeof(str) - len, "     ");
        }

        len += snprintf(str + len, sizeof(str) - len, " ");
        for (i = 0; i < num; ++i)
        {
            char c = line[i];
            len += snprintf(str + len, sizeof(str) - len, "%c", \
                    (c > (char)0x20 && c < (char)0x7f) ? c : '.');
        }

        if (n < size)
            len += snprintf(str + len, sizeof(str) - len, "\n");
    }

    return str;
}

char *addrtostr(const struct sockaddr_in *addr)
{
    static char str[30];
    str[0] = '\0';
    if (addr)
    {
        snprintf(str, sizeof(str), "%s:%u", \
                inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    }

    return str;
}

size_t hex_str(char *to, const char *data, size_t len)
{
    static char hex[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', 
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', };

    size_t i = 0;
    char *des = to;

    for (i = 0; i < len; ++i)
    {
        *des++ = hex[((uint8_t)(data[i]) & 0xf0) >> 4];
        *des++ = hex[(uint8_t)(data[i]) & 0x0f];
    }

    *des = '\0';

    return des - to;
}

char *get_hour_str(int offset, time_t *timeptr)
{
    static char hour_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;
    now += offset * 3600;

    struct tm *tm = localtime(&now);

    snprintf(hour_str, sizeof(hour_str), "%04d%02d%02d%02d", \
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour);

    return hour_str;
}

char *get_day_str(int offset, time_t *timeptr)
{
    static char day_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;
    now += offset * 3600 * 24;

    struct tm *tm = localtime(&now);

    snprintf(day_str, sizeof(day_str), "%04d%02d%02d", \
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    return day_str;
}

char *get_mon_str(int offset, time_t *timeptr)
{
    static char mon_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;

    struct tm *tm = localtime(&now);
    tm->tm_mon  += offset;
    tm->tm_year += (tm->tm_mon / 12);
    tm->tm_mon  %= 12;

    if (tm->tm_mon < 0)
    {
        tm->tm_mon  += 12;
        tm->tm_year -= 1;
    }

    snprintf(mon_str, sizeof(mon_str), "%04d%02d", \
            tm->tm_year + 1900, tm->tm_mon + 1);

    return mon_str;
}

char *get_year_str(int offset, time_t *timeptr)
{
    static char year_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;

    struct tm *tm = localtime(&now);
    tm->tm_year += offset;

    snprintf(year_str, sizeof(year_str), "%04d", tm->tm_year + 1900);

    return year_str;
}

char *get_date_str(int offset, time_t *timeptr)
{
    static char date_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;
    now += offset;

    struct tm *tm = localtime(&now);

    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", \
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    return date_str;
}

char *get_time_str(int offset, time_t *timeptr)
{
    static char time_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;
    now += offset;

    struct tm *tm = localtime(&now);

    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", \
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    return time_str;
}

char *get_datetime_str(int offset, time_t *timeptr)
{
    static char datetime_str[20];

    time_t now;
    if (timeptr == NULL)
        now = time(NULL);
    else
        now = *timeptr;
    now += offset;

    struct tm *tm = localtime(&now);

    snprintf(datetime_str, sizeof(datetime_str), \
            "%04d-%02d-%02d %02d:%02d:%02d", \
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, \
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    return datetime_str;
}

char *basepath(const char *path)
{
    static char name[NAME_MAX + 1];
    char *end = (char *)path + strlen(path) - 1;
    while (end >= path)
    {
        if (*end == '/')
        {
            ++end;
            break;
        }
        else
        {
            --end;
        }   
    }

    if (end == (char *)path - 1)
        end = (char *)path;

    return strncpy(name, end, sizeof(name) - 1);
}

char *parentpath(char *path)
{
    size_t len = strlen(path);
    size_t i;

    for (i = len; i > 1; --i)
    {
        if (path[i - 1] == '/')
        {
            path[i - 1] = 0;
            return path;
        }
    }

    if (path[0] == '/')
        path[1] = 0;

    return path;
}

uint64_t timeval_diff(struct timeval *old, struct timeval *new)
{
    return (new->tv_sec - old->tv_sec) * (1000 * 1000) -
        (old->tv_usec - new->tv_usec);
}

struct timeval *timeval_add(struct timeval *t, time_t usec)
{
    t->tv_usec += usec;
    t->tv_sec  += usec / 1000000;
    t->tv_usec %= 1000000;

    if (t->tv_usec < 0)
    {
        t->tv_usec += 1000000;
        t->tv_sec  -= 1;
    }

    return t;
}

/* copy from git/wrapper.c */
ssize_t xwrite(int fd, const void *buf, size_t len)
{
    ssize_t nr;
    while (1)
    {
        nr = write(fd, buf, len);
        if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
            continue;

        return nr;
    }
}

/* copy from git/wrapper.c */
ssize_t write_in_full(int fd, const void *buf, size_t count)
{
    const char *p = buf;
    ssize_t total = 0;

    while (count > 0)
    {
        ssize_t written = xwrite(fd, p, count);
        if (written < 0)
            return -1;
        if (!written)
        {
            errno = ENOSPC;
            return -1;
        }
        count -= written;
        p += written;
        total += written;
    }

    return total;
}

char *raw_input(const char *msg)
{
    printf("%s ", msg);
    fflush(stdout);

    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, stdin) == -1)
    {
        free(line);
        return NULL;
    }

    line[strlen(line) - 1] = '\0';

    return line;
}

bool ask_is_continue(void)
{
    char *answer = raw_input("\nContinue(Y/N)?");
    if (answer == NULL)
        return false;

    if (strlen(answer) == 1 && tolower(answer[0]) == 'y')
    {
        free(answer);
        return true;
    }

    free(answer);

    return false;
}

static void *__get_shm(key_t key, size_t size, int flag)
{
    int shm_id = shmget(key, size, flag);
    if (shm_id < 0)
        return NULL;

    void *p = shmat(shm_id, NULL, 0);
    if (p == (void *)-1)
        return NULL;

    return p;
}

int get_shm(key_t key, size_t size, void **addr)
{
    if ((*addr = __get_shm(key, size, 0666)) != NULL)
        return 0;

    if ((*addr = __get_shm(key, size, 0666 | IPC_CREAT)) != NULL)
        return 1;

    return -1;
}

size_t buf_sum(void const *p, size_t n)
{
    size_t sum = 0;
    size_t i;
    for (i = 0; i < n; ++i)
    {
        sum += ((uint8_t *)p)[i];
    }

    return sum;
}

