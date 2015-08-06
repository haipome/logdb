/*
 * Description: a log lib with cache, writen by damon
 *     History: damonyang@tencent.com, 2013/05/20, created
 */

# undef  _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 64

# include <stdio.h>
# include <stdint.h>
# include <string.h>
# include <stdlib.h>
# include <stdarg.h>
# include <fcntl.h>
# include <time.h>
# include <errno.h>
# include <limits.h>
# include <unistd.h>
# include <inttypes.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>

# include "dlog.h"

dlog_t *default_dlog = NULL;
int     default_dlog_flag = 0;

# define WRITE_INTERVAL_IN_USEC (10 * 1000)     /* 100 ms */
# define WRITE_BUFFER_CHECK_LEN (32 * 1024)     /* 32 KB */
# define WRITE_BUFFER_LEN       (64 * 1024)     /* 64 KB */

# ifdef DEBUG
static int write_times = 0;
# endif

/* all opened log is in a list, vist by lp_list_head */
static dlog_t *lp_list_head = NULL;

/* use to make sure dlog_atexit only call once */
static int init_flag = 0;

static char *log_suffix(int type, time_t sec, int i)
{
    static char str[30];

    if (type == DLOG_SHIFT_BY_SIZE)
    {
        if (i)
            snprintf(str, sizeof(str), "%d.log", i);
        else
            snprintf(str, sizeof(str), ".log");

        return str;
    }

    struct tm *t = localtime(&sec);
    ssize_t n = 0;

    switch (type)
    {
    case DLOG_SHIFT_BY_MIN:
        n = snprintf(str, sizeof(str), "_%04d%02d%02d%02d%02d",
                t->tm_year + 1900, t->tm_mon + 1,
                t->tm_mday, t->tm_hour, t->tm_min);

        break;
    case DLOG_SHIFT_BY_HOUR:
        n = snprintf(str, sizeof(str), "_%04d%02d%02d%02d",
                t->tm_year + 1900, t->tm_mon + 1,
                t->tm_mday, t->tm_hour);

        break;
    case DLOG_SHIFT_BY_DAY:
        n = snprintf(str, sizeof(str), "_%04d%02d%02d",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
        break;
    default:
        str[0] = 0;

        break;
    }

    if (i)
        snprintf(str + n, sizeof(str) - n, "_%d.log", i);
    else
        snprintf(str + n, sizeof(str) - n, ".log");

    return str;
}

static inline uint64_t timeval_diff(struct timeval *old, struct timeval *new)
{
    return (new->tv_sec - old->tv_sec) * (1000 * 1000) -
        (old->tv_usec - new->tv_usec);
}

static int _unlink_expire(dlog_t *lp, time_t expire_time)
{
    char path[PATH_MAX];
    int  num = 0;
    int  i;

    if (lp->log_num == 0)
    {
        for (i = 0;; ++i)
        {
            snprintf(path, PATH_MAX, "%s%s", lp->base_name,
                    log_suffix(lp->shift_type, expire_time, i));
            if (access(path, F_OK) == 0)
                ++num;
            else
                break;
        }
    }
    else
    {
        num = lp->log_num;
    }

    for (i = 0; i < num; ++i)
    {
        snprintf(path, PATH_MAX, "%s%s", lp->base_name,
                log_suffix(lp->shift_type, expire_time, i));

        unlink(path);
    }

    return 0;
}

static int unlink_expire(dlog_t *lp, struct timeval *now)
{
    if (lp->shift_type == DLOG_SHIFT_BY_SIZE || lp->keep_time == 0)
        return 0;

    int expire = 0;
    time_t expire_time;

    switch (lp->shift_type)
    {
    case DLOG_SHIFT_BY_MIN:
        if (now->tv_sec - lp->last_unlink > 60)
        {
            expire = 1;
            expire_time = now->tv_sec - 60 * lp->keep_time;
        }

        break;
    case DLOG_SHIFT_BY_HOUR:
        if (now->tv_sec - lp->last_unlink > 3600)
        {
            expire = 1;
            expire_time = now->tv_sec - 3600 * lp->keep_time;
        }

        break;
    case DLOG_SHIFT_BY_DAY:
        if (now->tv_sec - lp->last_unlink > 86400)
        {
            expire = 1;
            expire_time = now->tv_sec - 86400 * lp->keep_time;
        }

        break;
    }

    if (expire)
    {
        lp->last_unlink = now->tv_sec;
# ifdef DEBUG
        struct timeval start, end;
        gettimeofday(&start, NULL);
# endif
        if (lp->use_fork)
        {
            if (fork() == 0)
            {
                _unlink_expire(lp, expire_time);
                _exit(0);
            }
        }
        else
        {
            _unlink_expire(lp, expire_time);
        }
# ifdef DEBUG
        gettimeofday(&end, NULL);
        printf("unlink time: %"PRIu64"\n", timeval_diff(&start, &end));
# endif
    }

    return 0;
}

static char *log_name(dlog_t *lp, struct timeval *now)
{
    sprintf(lp->name, "%s%s", lp->base_name,
            log_suffix(lp->shift_type, now->tv_sec, 0));

    return lp->name;
}

static int _shift_log(dlog_t *lp, struct timeval *now)
{
    if (lp->log_num == 1)
    {
        unlink(lp->name);
        return 0;
    }

    char path[PATH_MAX];
    char new_path[PATH_MAX];
    int  num = 0;
    int  i;

    if (lp->log_num == 0)
    {
        for (i = 0;; ++i)
        {
            snprintf(path, PATH_MAX, "%s%s", lp->base_name,
                    log_suffix(lp->shift_type, now->tv_sec, i));
            if (access(path, F_OK) == 0)
                ++num;
            else
                break;
        }
    }
    else
    {
        num = lp->log_num - 1;
    }

    for (i = num - 1; i >= 0; --i)
    {
        snprintf(path, PATH_MAX, "%s%s", lp->base_name,
                log_suffix(lp->shift_type, now->tv_sec, i));

        if (access(path, F_OK) == 0)
        {
            snprintf(new_path, PATH_MAX, "%s%s", lp->base_name,
                    log_suffix(lp->shift_type, now->tv_sec, i + 1));

            rename(path, new_path);
        }
    }

    return 0;
}

static int shift_log(dlog_t *lp, struct timeval *now)
{
    if (lp->max_size == 0)
        return 0;

    if (lp->use_fork && ((now->tv_sec - lp->last_shift) <= 3))
        return 0;

    struct stat fs;
    if (stat(lp->name, &fs) < 0)
    {
        if (errno == ENOENT)
            return 0;
        else
            return -1;
    }

    int ret = 0;

    if ((size_t)fs.st_size >= lp->max_size)
    {
        lp->last_shift = now->tv_sec;
# ifdef DEBUG
        struct timeval start, end;
        gettimeofday(&start, NULL);
# endif
        if (lp->use_fork)
        {
            if (fork() == 0)
            {
                _shift_log(lp, now);
                _exit(0);
            }
        }
        else
        {
            _shift_log(lp, now);
        }
# ifdef DEBUG
        gettimeofday(&end, NULL);
        printf("shift time: %"PRIu64"\n", timeval_diff(&start, &end));
# endif
    }

    return ret;
}

static ssize_t xwrite(int fd, const void *buf, size_t len)
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

static ssize_t write_in_full(int fd, const void *buf, size_t count)
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

static int flush_log(dlog_t *lp, struct timeval *now)
{
    if (lp->w_len == 0)
        return 0;

    ssize_t n = 0;
    int ret_val = 0;

    if (lp->remote_log)
    {
        /* lasz init, because dlog_init may call befor daemon */
        if (lp->sockfd == 0)
        {
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0)
            {
                ret_val = -1;
                goto end_lable;
            }
            lp->sockfd = sockfd;
        }

        n = sendto(lp->sockfd, lp->buf, lp->w_len, 0, (struct sockaddr *)&lp->addr, sizeof(lp->addr));
        if (n < 0)
        {
            if (errno == EBADF)
                lp->sockfd = 0;
            ret_val = -1;
            goto end_lable;
        }
    }
    else
    {
        log_name(lp, now);
        shift_log(lp, now);
        unlink_expire(lp, now);

        int fd = open(lp->name, O_WRONLY | O_APPEND | O_CREAT, 0664);
        if (fd < 0)
        {
            ret_val = -1;
            goto end_lable;
        }
        n = write_in_full(fd, lp->buf, lp->w_len);
        close(fd);
        if (n < 0)
        {
            ret_val = -1;
            goto end_lable;
        }
    }

end_lable:
    lp->w_len = 0;
# ifdef DEBUG
    ++write_times;
# endif
    lp->last_write = *now;

    return ret_val;
}

static void dlog_atexit(void)
{
    dlog_t *lp = lp_list_head;

    while (lp)
    {
        dlog_t *_lp = lp;
        lp = (dlog_t *)lp->next;
        dlog_fini(_lp);
    }
}

static void *dlog_free(dlog_t *lp)
{
    free(lp->base_name);
    free(lp->name);
    free(lp->buf);
    free(lp);

    return NULL;
}

static int is_remote_log(char *base_name, struct sockaddr_in *addr)
{
    if (strchr(base_name, ':'))
    {
        char name[PATH_MAX] = { 0 };
        strncpy(name, base_name, sizeof(name) - 1);

        char *ip = strtok(name, ": \t\n\r");
        char *port = strtok(NULL, ": \t\n\r");

        if (ip == NULL || port == NULL)
            return -1;

        memset(addr, 0, sizeof(struct sockaddr_in));
        addr->sin_family = AF_INET;
        if (inet_aton(ip, &addr->sin_addr) == 0)
            return -1;
        addr->sin_port = htons((unsigned short)atoi(port));
        if (addr->sin_port == 0)
            return -1;

        return 1;
    }

    return 0;
}

dlog_t *dlog_init(char *base_name, int flag,
        size_t max_size, int log_num, int keep_time)
{
    if (base_name == NULL)
        return NULL;

    int use_fork = flag & DLOG_USE_FORK;
    flag &= ~DLOG_USE_FORK;

    int no_cache = flag & DLOG_NO_CACHE;
    flag &= ~DLOG_NO_CACHE;

    int remote_log = flag & DLOG_REMOTE_LOG;
    flag &= ~DLOG_REMOTE_LOG;

    dlog_t *lp = calloc(1, sizeof(dlog_t));
    if (lp == NULL)
        return NULL;

    if (remote_log)
    {
        lp->addr = *((struct sockaddr_in *)base_name);
    }
    else
    {
        if ((remote_log = is_remote_log(base_name, &lp->addr)) < 0)
            return dlog_free(lp);
        if (remote_log == 0)
        {
            if ((lp->base_name = strdup(base_name)) == NULL)
                return dlog_free(lp);
        }
    }

    if (!remote_log && (flag < DLOG_SHIFT_BY_SIZE || flag > DLOG_SHIFT_BY_DAY))
        return NULL;

    lp->name = malloc(strlen(base_name) + 30);
    lp->buf_len = WRITE_BUFFER_LEN;
    lp->buf = malloc(lp->buf_len);
    if (lp->name == NULL || lp->buf == NULL)
        return dlog_free(lp);

    lp->shift_type = flag;
    lp->use_fork   = use_fork;
    lp->no_cache   = no_cache;
    lp->max_size   = max_size;
    lp->log_num    = log_num;
    lp->keep_time  = keep_time;
    lp->remote_log = remote_log;

    struct timeval now;
    gettimeofday(&now, NULL);
    lp->last_write = now;

    pthread_mutex_init(&lp->lock, NULL);

    if (init_flag == 0)
    {
        atexit(dlog_atexit);
        init_flag = 1;
    }

    if (!lp->remote_log)
    {
        int fd = open(log_name(lp, &now), O_WRONLY | O_APPEND | O_CREAT, 0664);
        if (fd < 0)
            return dlog_free(lp);
        close(fd);
    }

    if (lp_list_head == NULL)
    {
        lp_list_head = lp;
    }
    else
    {
        dlog_t *_lp = lp_list_head;
        while (_lp->next)
            _lp = (dlog_t *)_lp->next;
        _lp->next = (void *)lp;
    }

    return lp;
}

static inline void _dlog_check(dlog_t *lp, struct timeval *now)
{
    if ((timeval_diff(&lp->last_write, now) >= WRITE_INTERVAL_IN_USEC) ||
            (lp->w_len >= WRITE_BUFFER_CHECK_LEN))
    {
        flush_log(lp, now);
    }
}

void dlog_check(dlog_t *lp, struct timeval *tv)
{
    if (lp)
    {
        if (lp->w_len == 0)
            return;
    }
    else
    {
        lp = lp_list_head;
        while (lp)
        {
            if (lp->w_len)
                break;
            lp = (dlog_t *)lp->next;
        }
        if (lp == NULL)
            return;
    }

    struct timeval now;

    if (tv == NULL)
    {
        gettimeofday(&now, NULL);
        tv = &now;
    }

    if (lp)
    {
        _dlog_check(lp, tv);
    }
    else
    {
        lp = lp_list_head;
        while (lp)
        {
            if (lp->w_len)
                _dlog_check(lp, tv);
            lp = (dlog_t *)lp->next;
        }
    }
}

static char *timeval_str(struct timeval *tv)
{
    static char str[64];

    struct tm *t = localtime(&tv->tv_sec);
    snprintf(str, sizeof(str), "%04d-%02d-%02d %02d:%02d:%02d.%.6d",
            t->tm_year + 1990, t->tm_mon + 1, t->tm_mday, t->tm_hour,
            t->tm_min, t->tm_sec, (int)tv->tv_usec);

    return str;
}

static int _dlog(dlog_t *lp, const char *fmt, va_list ap) 
{
    if (!lp || !fmt)
        return -1;

    struct timeval now;
    gettimeofday(&now, NULL);
    char *timestmap = timeval_str(&now);

    ssize_t n  = 0;
    ssize_t ret;

    char *p = lp->buf + lp->w_len;
    size_t len = lp->buf_len - lp->w_len;

    ret = snprintf(p, len, "[%s] ", timestmap);
    if (ret < 0)
        return -1;

    p += ret;
    len -= ret;
    n += ret;

    va_list cap;
    va_copy(cap, ap);
    ret = vsnprintf(p, len, fmt, cap);

    if (ret < 0)
    {
        return -1;
    }
    else if ((size_t)ret >= len)
    {
        if (lp->remote_log)
        {
            /* drop the remaining */
            n += len - 1;
            lp->w_len += n;
            flush_log(lp, &now);
        }
        else
        {
            flush_log(lp, &now);

            FILE *fp = fopen(log_name(lp, &now), "a+");
            if (fp == NULL)
                return -2;

            fprintf(fp, "[%s] ", timestmap);
            vfprintf(fp, fmt, ap);
            fprintf(fp, "\n");

            fclose(fp);
        }
    }
    else
    {
        n += ret;
        lp->w_len += n;

        lp->buf[lp->w_len] = '\n';
        lp->w_len += 1;

        if (lp->no_cache)
            flush_log(lp, &now);
        else
            _dlog_check(lp, &now);
    }

    return 0;
}

int dlog(dlog_t *lp, const char *fmt, ...)
{
    pthread_mutex_lock(&lp->lock);
    va_list ap;
    va_start(ap, fmt);
    int ret = _dlog(lp, fmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&lp->lock);

    return ret;
}

# ifdef DLOG_SERVER
int dlog_server(dlog_t *lp, char *buf, size_t size, struct sockaddr_in *addr)
{
    if (lp->remote_log)
        return -1;

    struct timeval now;
    gettimeofday(&now, NULL);

    log_name(lp, &now);
    shift_log(lp, &now);
    unlink_expire(lp, &now);

    int fd = open(lp->name, O_WRONLY | O_APPEND | O_CREAT, 0664);
    if (fd < 0)
        return -2;

    ssize_t n;
    if (addr)
    {
        char addr_str[30];
        int len = snprintf(addr_str, sizeof(addr_str), "[%s:%u]\n", \
                inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));

        n = write_in_full(fd, addr_str, len);
        if (n < 0)
        {
            close(fd);
            return -3;
        }
    }
    n = write_in_full(fd, buf, size);
    if (n < 0)
    {
        close(fd);
        return -4;
    }

    close(fd);

    return 0;
}
# endif

int dlog_fini(dlog_t *lp)
{
    if (lp == lp_list_head)
    {
        lp_list_head = (dlog_t *)lp->next;
    }
    else
    {
        dlog_t *_lp = lp_list_head;
        while (_lp)
        {
            if (_lp->next == lp)
            {
                _lp->next = lp->next;
                break;
            }
            else
            {
                _lp = (dlog_t *)_lp->next;
            }
        }

        return -1; /* not found */
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    flush_log(lp, &now);

    dlog_free(lp);
# ifdef DEBUG
    printf("write %d times\n", write_times);
# endif

    return 0;
}

static char *strtolower(char *str)
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

int dlog_read_flag(char *str)
{
    if (str == NULL)
        return 0;

    int flag = 0;

    char *s = strdup(str);
    if (s == NULL) 
        return -1;

    char *f = strtok(s, "\r\n\t ,");
    while (f != NULL)
    {
        strtolower(f);

        if (strcmp(f, "fatal") == 0)
            flag |= DLOG_FATAL;
        else if (strcmp(f, "error") == 0)
            flag |= DLOG_ERROR;
        else if (strcmp(f, "warn") == 0)
            flag |= DLOG_WARN;
        else if (strcmp(f, "info") == 0)
            flag |= DLOG_INFO;
        else if (strcmp(f, "notice") == 0)
            flag |= DLOG_NOTICE;
        else if (strcmp(f, "debug") == 0)
            flag |= DLOG_DEBUG;
        else if (strcmp(f, "user1") == 0)
            flag |= DLOG_USER1;
        else if (strcmp(f, "user2") == 0)
            flag |= DLOG_USER2;

        f = strtok(NULL, "\r\n\t ,");
    }

    free(s);

    return flag;
}

