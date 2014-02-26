/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/18, create
 */


# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <inttypes.h>
# include <errno.h>
# include <math.h>
# include <unistd.h>
# include <netinet/in.h>

# include "serialize.h"
# include "queue.h"
# include "net.h"
# include "conf.h"
# include "utils.h"
# include "db.h"
# include "job.h"
# include "sql.h"
# include "seq.h"
# include "protocol.h"

extern int shut_down_flag;

static int recv_pkg_count;
static int process_pkg_succ_count;
static int process_pkg_fail_count;
static int insert_db_succ_count;
static int insert_db_fail_count;
static int exec_sql_succ_count;
static int exec_sql_fail_count;

static int choice_worker(void)
{
    static int last_worker = 0;

    int least = 0;
    uint64_t least_num = UINT64_MAX;

    int i;
    for (i = 1; i <= settings.worker_proc_num; ++i)
    {
        int worker_id = (last_worker - 1 + i) % settings.worker_proc_num + 1;
        queue_t *queue = &settings.workers[worker_id].queue;
        uint64_t num = queue_num(queue);

        if (num == 0)
        {
            last_worker = worker_id;

            return worker_id;
        }

        if (num < least_num)
        {
            least = worker_id;
            least_num = num;
        }
    }

    last_worker = least;

    return least;
}

static int push_sql(char *sql, size_t len)
{
    int worker_id = 1;
    if (settings.worker_proc_num > 1)
        worker_id = choice_worker();
    struct worker *worker = &settings.workers[worker_id];

    if (queue_push(&worker->queue, sql, len) < 0)
    {
        log_error("push sql fail, worker_id: %d", worker_id);
        dlog(settings.fail_enqueue_log, "%s;", sql);

        return -1;
    }

    write(worker->pipefd[1], "", 1);

    return 0;
}

static int flush_table(struct table *table)
{
    if (table->buf_use == 0)
        return 0;

    int ret = push_sql(table->buf, table->buf_use + 1);
    table->buf_use = 0;

    if (ret < 0)
        return ret;

    return 0;
}

static void reciver_looper(void)
{
    if (shut_down_flag)
    {
        int i;
        for (i = 0; i < settings.hash_table_num; ++i)
        {
            flush_table(&settings.tables[i]);
        }

        log_vip("reciver, shut down...");

        exit(0);
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    dlog_check(NULL, &now);

    static time_t last_log_min;
    time_t curr_min = now.tv_sec / 60;
    if (curr_min != last_log_min)
    {
        if (last_log_min != 0)
        {
            log_info("reciver: recv pkg: %d, succ: %d, fail: %d", \
                    recv_pkg_count, process_pkg_succ_count, process_pkg_fail_count);

            recv_pkg_count = 0;
            process_pkg_succ_count = 0;
            process_pkg_fail_count = 0;
        }

        last_log_min = curr_min;
    }

    static struct timeval last_check;
    if (settings.cache_time_in_ms && ((timeval_diff(&last_check, &now) / 1000) > (uint64_t)settings.check_time_in_ms))
    {
        int i;
        for (i = 0; i < settings.hash_table_num; ++i)
        {
            if (settings.tables[i].buf_use &&
                    (timeval_diff(&settings.tables[i].start, &now) / 1000) > (uint64_t)settings.cache_time_in_ms)
            {
                flush_table(&settings.tables[i]);
            }
        }

        last_check = now;
    }

    return;
}

# define FAIL_LOG(x) do { \
    int __ret = (x); \
    if (__ret < 0 && !(__ret == -2 && left == 0)) { \
        log_error("fail when parse column: %s, ret code: %d, offset: %u, pkg len: %u\n%s", \
                curr->name, __ret, len - left, len, hex_dump_str(pkg, len)); \
        return NULL; \
    } \
} while (0)

# define GET_INT(ut, priu, it, prii) do { \
    if (curr->is_zero) { \
        if (curr->is_storage) { \
            use += lstrncpy(str + use, "0", sizeof(str) - use); \
        } \
    } else if (curr->is_auto_increment) {\
        if (curr->is_storage) { \
            use += lstrncpy(str + use, "NULL", sizeof(str) - use); \
        } \
    } else if (curr->is_current_timestamp) { \
        it##_t v = (it##_t)time(NULL); \
        if (curr->is_storage) { \
            use += snprintf(str + use, sizeof(str) - use, "%"prii, v); \
        } \
    } else if (curr->is_global_sequence) { \
        ut##_t v = (ut##_t)sequence_get(); \
        if (curr->is_storage) { \
            use += snprintf(str + use, sizeof(str) - use, "%"priu, v); \
        } \
    } else if (curr->is_sender_ip) { \
        ut##_t v = (ut##_t)ntohl(client_addr->sin_addr.s_addr); \
        if (curr->is_storage) { \
            use += snprintf(str + use, sizeof(str) - use, "%"priu, v); \
        } \
    } else if (curr->is_sender_port) { \
        ut##_t v = (ut##_t)ntohs(client_addr->sin_port); \
        if (curr->is_storage) { \
            use += snprintf(str + use, sizeof(str) - use, "%"priu, v); \
        } \
    } else { \
        if (curr->is_unsigned) { \
            ut##_t v = 0; \
            FAIL_LOG(get_##ut(&p, &left, &v)); \
            if (settings.hash_table_column == curr) { \
                *hash_key = (uint64_t)v; \
            } \
            if (curr->is_storage) { \
                use += snprintf(str + use, sizeof(str) - use, "%"priu, v); \
            } \
        } else if (!curr->is_unsigned) { \
            it##_t v = 0; \
            FAIL_LOG(get_##it(&p, &left, &v)); \
            if (settings.hash_table_column == curr) { \
                *hash_key = (uint64_t)v; \
            } \
            if (curr->is_storage) { \
                use += snprintf(str + use, sizeof(str) - use, "%"prii, v); \
            } \
        } \
    } \
    if (curr->is_storage) {\
        use += lstrncpy(str + use, ",", sizeof(str) - use); \
    } \
} while (0)

# define GET_STR(fun) do { \
    size_t vlen = 0; \
    if (curr->is_zero) { \
        vlen = 0; \
    } else if (curr->is_const_length) { \
        if (auto_realloc(&buf, &buf_len, curr->length + 1) == NULL) { \
            return NULL; \
        } \
        FAIL_LOG(ret = get_bin(&p, &left, buf, curr->length)); \
        if (ret < 0) { \
            vlen = 0; \
        } else { \
            ((char *)buf)[curr->length] = 0; \
            vlen = strlen((char *)buf); \
        } \
    } else { \
        if (curr->is_zero_end) { \
            FAIL_LOG(ret = get_str(&p, &left, (char **)&buf, &buf_len)); \
        } else { \
            FAIL_LOG(ret = fun(&p, &left, (char **)&buf, &buf_len)); \
        } \
        if (ret < 0) { \
            vlen = 0; \
        } else { \
            vlen = strlen((char *)buf); \
        } \
        if (vlen > curr->length) { \
            log_error("column: %s, length %zu is greate than max length: %u", \
                    curr->name, vlen, curr->length); \
            vlen = curr->length; \
        } \
    } \
    ((char *)buf)[vlen] = 0; \
    if (curr->is_storage) { \
        use += lstrncpy(str + use, "'", sizeof(str) - use); \
        use += db_escape_string(str + use, (char *)buf, vlen); \
        use += lstrncpy(str + use, "',", sizeof(str) - use); \
    } \
} while (0)

# define GET_BIN(fun) do { \
    size_t vlen = 0; \
    if (curr->is_zero) { \
        vlen = 0; \
    } else if (curr->is_const_length) { \
        if (auto_realloc(&buf, &buf_len, curr->length) == NULL) { \
            return NULL; \
        } \
        memset(buf, 0, curr->length); \
        FAIL_LOG(get_bin(&p, &left, buf, curr->length)); \
        vlen = curr->length; \
    } else { \
        FAIL_LOG(ret = fun(&p, &left, &buf, &buf_len)); \
        if (ret < 0) { \
            vlen = 0; \
        } else { \
            vlen = ret; \
        } \
        if (vlen > curr->length) { \
            log_error("colunm: %s, length %zu is greate than max length: %u", \
                    curr->name, vlen, curr->length); \
            vlen = curr->length; \
        } \
    } \
    if (curr->is_storage) { \
        use += lstrncpy(str + use, "unhex('", sizeof(str) - use); \
        use += hex_str(str + use, (char *)buf, vlen); \
        use += lstrncpy(str + use, "'),", sizeof(str) - use); \
    } \
} while (0)

# define GET_TIME(fun) do { \
    char const *v = NULL; \
    if (curr->is_zero) { \
        v = "0"; \
    } else if (curr->is_current_timestamp) { \
        v = fun(0, NULL); \
    } else if (curr->is_unix_timestamp) { \
        int64_t bt = 0; \
        FAIL_LOG(get_int64(&p, &left, &bt)); \
        time_t t = (time_t)bt; \
        v = fun(0, &t); \
    } else { \
        FAIL_LOG(get_str1(&p, &left, (char **)&buf, &buf_len)); \
        if (ret < 0) { \
            v = "0"; \
        } else { \
            v = buf; \
        } \
    } \
    if (curr->is_storage) { \
        size_t vlen = strlen(v); \
        use += lstrncpy(str + use, "'", sizeof(str) - use); \
        use += db_escape_string(str + use, v, vlen); \
        use += lstrncpy(str + use, "',", sizeof(str) - use); \
    } \
} while (0)

static char *pkgtostr(struct sockaddr_in *client_addr, char *pkg, int len, uint64_t *hash_key)
{
    int ret;

    void *p  = pkg;
    int left = len;

    static char str[UINT16_MAX * 10];
    size_t use = 0;

    static void  *buf;
    static size_t buf_len;

    /* make sure buf is not NULL */
    auto_realloc(&buf, &buf_len, 128);

    struct column *curr = settings.columns;
    while (curr)
    {
        switch (curr->type)
        {
        case COLUMN_TYPE_TINY_INT:
            GET_INT(uint8, PRIu8, int8, PRIi8);

            break;
        case COLUMN_TYPE_SMALL_INT:
            GET_INT(uint16, PRIu16, int16, PRIi16);

            break;
        case COLUMN_TYPE_INT:
            GET_INT(uint32, PRIu32, int32, PRIi32);

            break;
        case COLUMN_TYPE_BIG_INT:
            GET_INT(uint64, PRIu64, int64, PRIi64);

            break;
        case COLUMN_TYPE_FLOAT:
            {
                float v = 0.0;
                if (curr->is_zero == false)
                    FAIL_LOG(get_float(&p, &left, &v));
                if (curr->is_storage)
                    use += snprintf(str + use, sizeof(str) - use, "%.7g,", v);
            }

            break;
        case COLUMN_TYPE_DOUBLE:
            {
                double v = 0.0;
                if (curr->is_zero == false)
                    FAIL_LOG(get_double(&p, &left, &v));
                if (curr->is_storage)
                    use += snprintf(str + use, sizeof(str) - use, "%.16g,", v);
            }

            break;
        case COLUMN_TYPE_CHAR:
            GET_STR(get_str1);

            break;
        case COLUMN_TYPE_VARCHAR:
            GET_STR(get_str2);

            break;
        case COLUMN_TYPE_TINY_TEXT:
            GET_STR(get_str1);

            break;
        case COLUMN_TYPE_TEXT:
            GET_STR(get_str2);

            break;
        case COLUMN_TYPE_BINARY:
            GET_BIN(get_bin1);

            break;
        case COLUMN_TYPE_VARBINARY:
            GET_BIN(get_bin2);

            break;
        case COLUMN_TYPE_TINY_BLOB:
            GET_BIN(get_bin1);

            break;
        case COLUMN_TYPE_BLOB:
            GET_BIN(get_bin2);

            break;
        case COLUMN_TYPE_DATE:
            GET_TIME(get_date_str);

            break;
        case COLUMN_TYPE_TIME:
            GET_TIME(get_time_str);

            break;
        case COLUMN_TYPE_DATETIME:
            GET_TIME(get_datetime_str);

            break;
        default:
            log_fatal("unknow column type: %d", curr->type);

            break;
        }

        curr = curr->next;
    }

    /* delete last comma */
    use -= 1;
    str[use] = 0;

    return str;
}

static int choice_table(uint64_t hash_key)
{
    return abs(hash_key % settings.hash_table_num);
}

static int process_one_record(char *s, uint64_t hash_key)
{
    int table_id = choice_table(hash_key);
    struct table *table = &settings.tables[table_id];
    bool is_first = false;

    size_t record_len = strlen(s);
    if (table->buf_use && ((table->buf_use + record_len + 5) >= (size_t)settings.cache_len))
    {
        flush_table(table);
    }

    if (table->buf_use == 0)
    {
        is_first = true;

# define FMT_INSERT "INSERT INTO `%s` (%s) VALUES"
        char *table_name = get_table_name(table_id, 0);
        size_t base_len = strlen(FMT_INSERT) + strlen(table_name) + settings.columns_str_len;
        if (auto_realloc((void **)&table->buf, &table->buf_len, base_len) == NULL)
            return -__LINE__;
        table->buf_use = snprintf(table->buf, table->buf_len, FMT_INSERT, table_name, settings.columns_str);
# undef FMT_INSERT

        if (settings.cache_time_in_ms)
        {
            gettimeofday(&table->start, NULL);

            if (table->not_first == false)
            {
                int slat = (settings.cache_time_in_ms + \
                        settings.hash_table_num - 1) / settings.hash_table_num;
                timeval_add(&table->start, \
                        - 1 * (settings.hash_table_num - table_id - 1) * slat * 1000);

                table->not_first = true;
            }
        }
    }

    if (auto_realloc((void **)&table->buf, &table->buf_len, table->buf_use + record_len + 5) == NULL)
        return -__LINE__;

    table->buf_use += snprintf(table->buf + table->buf_use, table->buf_len - table->buf_use, \
            "%s (%s)", is_first ? "" : ",", s);

    bool is_push = false;
    if (settings.cache_time_in_ms == 0)
    {
        is_push = true;
    }
    else
    {
        if (table->buf_use >= (size_t)settings.cache_len)
        {
            is_push = true;
        }
        else if (!is_first)
        {
            struct timeval now;
            gettimeofday(&now, NULL);

            if ((timeval_diff(&table->start, &now) / 1000) > (uint64_t)settings.cache_time_in_ms)
            {
                is_push = true;
            }
        }
    }

    if (is_push)
    {
        flush_table(table);
    }

    return 0;
}

static int reply(struct protocol_head *head, struct sockaddr_in *addr, uint8_t result)
{
    if (result == RESULT_OK)
    {
        ++process_pkg_succ_count;
    }
    else
    {

        ++process_pkg_fail_count;
    }

    head->result = result;

    char buf[head->echo_len + 10];
    char *p = buf;
    int left = head->echo_len + 10;

    NEG_RET_LN(add_head(head, (void **)&p, &left));
    NEG_RET_LN(send_udp_pkg(buf, p - buf, addr));

    return 0;
}

static int handle_udp(struct sockaddr_in *client_addr, char *pkg, int len)
{
    ++recv_pkg_count;

    log_debug("recv pkg from %s, len: %d\n%s", addrtostr(client_addr), len, \
            hex_dump_str(pkg, len));

    int ret;

    char *p = pkg;
    int left = len;

    struct protocol_head head;
    ret = get_head(&head, (void **)&p, &left);
    if (ret < 0)
        return -__LINE__;

    if (head.command == COMMAND_SQL)
    {
        ret = push_sql(p, left);
        if (ret < 0)
        {
            log_error("push to queue fail: %d", ret);
            NEG_RET(reply(&head, client_addr, RESULT_INTERNAL_ERROR));

            return -__LINE__;
        }
    }
    else
    {
        struct sockaddr_in real_client_addr;
        if (head.echo_len == sizeof(struct inner_addr))
        {
            struct inner_addr *addr = (struct inner_addr *)head.echo;
            bzero(&real_client_addr, sizeof(real_client_addr));
            real_client_addr.sin_addr.s_addr = addr->ip;
            real_client_addr.sin_port        = addr->port;
        }

        uint64_t hash_key = 0;
        char *s = pkgtostr(&real_client_addr, p, left, &hash_key);
        if (s == NULL)
        {
            NEG_RET(reply(&head, client_addr, RESULT_PKG_FMT_ERROR));

            return -__LINE__;
        }

        ret = process_one_record(s, hash_key);
        if (ret < 0)
        {
            log_error("process one record fail: %d", ret);
            NEG_RET(reply(&head, client_addr, RESULT_INTERNAL_ERROR));

            return -__LINE__;
        }
    }

    NEG_RET(reply(&head, client_addr, RESULT_OK));

    return 0;
}

int do_reciver_job(void)
{
    while (true)
    {
        reciver_looper();

        char pkg[UINT16_MAX];
        int  len = 0;
        struct sockaddr_in client_addr;

        errno = 0;
        int ret = 0;

        ret = recv_udp_pkg(&client_addr, pkg, sizeof(pkg), &len);
        if (ret < -1)
        {
            if (errno)
                log_error("recv udp pkg error: %d: %m", ret);
            else
                log_error("resv dup pkg error: %d", ret);
        }

        if (ret < 0)
        {
            continue;
        }

        ret = handle_udp(&client_addr, pkg, len);
        if (ret < 0)
        {
            log_error("handle udp pkg fail: %d", ret);
        }
    }

    return 0;
}

static int create_new_tables(void)
{
    int i;
    for (i = 0; i < settings.hash_table_num; ++i)
    {
        char *table_name = get_table_name(i, 0);
        if (table_name == NULL)
            return -__LINE__;

        char *create_sql = create_table_sql(table_name);
        if (create_sql == NULL)
            return -__LINE__;

        log_debug("worker: %d, sql: %s", settings.worker_id, create_sql);

        int ret = db_query(create_sql, 0);
        if (ret < 0)
        {
            log_error("exec sql fail: %s, sql: %s", db_error(), create_sql);

            return -__LINE__;
        }
    }

    if (settings.hash_table_num > 1 && strcasecmp(settings.db_engine, "myisam") == 0)
    {
        char *merge_sql = create_merge_table_sql(0);
        if (merge_sql == NULL)
            return -__LINE__;

        log_debug("worker: %d, sql: %s", settings.worker_id, merge_sql);

        int ret = db_query(merge_sql, 0);
        if (ret < 0)
        {
            log_error("exec sql fail: %s, sql: %s", db_error(), merge_sql);

            return -__LINE__;
        }
    }

    return 0;
}

static int drop_table(char *table_name)
{
    static char *sql = NULL;
    static size_t sql_buf_len = 0;

# define FMT_DROP "DROP TABLE IF EXISTS `%s`"
    if ((sql = auto_realloc((void **)&sql, &sql_buf_len, strlen(FMT_DROP) + strlen(table_name))) == NULL)
        return -__LINE__;
    snprintf(sql, sql_buf_len, FMT_DROP, table_name);
# undef FMT_DROP

    log_debug("worker: %d, sql: %s", settings.worker_id, sql);

    int ret = db_query(sql, 0);
    if (ret < 0)
    {
        log_error("exec sql fail: %s, sql: %s", db_error(), sql);

        return -__LINE__;
    }

    return 0;
}

static int drop_expire_tables(void)
{
    if (settings.data_keep_time == 0)
        return 0;

    int i;
    for (i = 0; i < settings.hash_table_num; ++i)
    {
        char *table_name = get_table_name(i, -settings.data_keep_time);
        if (table_name == NULL)
            return -__LINE__;

        NEG_RET(drop_table(table_name));
    }

    if (settings.hash_table_num > 1 && strcasecmp(settings.db_engine, "myisam") == 0)
    {
        char *merge_table = get_table_name(-1, -settings.data_keep_time);
        if (merge_table == NULL)
            return -__LINE__;

        NEG_RET(drop_table(merge_table));
    }

    return 0;
}

static void check_shift(time_t tv_sec)
{
    int curr = 0;
    static int last_create = -1;
    static int last_drop   = -1;

    /*
     * BUG 2014-01-20
     *
     * 由按天分表改为按月分表后，重启后没有创建新的表，原因
     * 是 struct tm 的月份是从 0 开始的，当前时间为 1 月，
     * tm_mon 为 0, 而 last_create 初始值为 0. 按小时分表
     * 也有类似的问题。
     *
     * 修复：last_create 和 last_drop 初始化为 -1.
     */

    struct tm *tm = localtime(&tv_sec);
    switch (settings.shift_table_type)
    {
    case TABLE_SHIFT_BY_HOUR:
        curr = tm->tm_hour;
        break;
    case TABLE_SHIFT_BY_DAY:
        curr = tm->tm_mday;
        break;
    case TABLE_SHIFT_BY_MON:
        curr = tm->tm_mon;
        break;
    case TABLE_SHIFT_BY_YEAR:
        curr = tm->tm_year;
        break;
    default:
        log_fatal("unknow shift type: %d", settings.shift_table_type);
        break;
    }

    if (curr != last_create)
    {
        int ret = create_new_tables();
        if (ret < 0)
        {
            log_error("create new tables fail: %d", ret);
        }
        else
        {
            last_create = curr;
        }
    }

    if (settings.shift_table_type != TABLE_NO_SHIFT && settings.data_keep_time && curr != last_drop)
    {
        int ret = drop_expire_tables();
        if (ret < 0)
        {
            log_error("drop expire tables fail: %d", ret);
        }

        last_drop = curr;
    }

    return;
}

static void worker_looper(void)
{
    if (shut_down_flag)
    {
        log_vip("worker id: %d, shut down...", settings.worker_id);

        exit(0);
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    dlog_check(NULL, &now);

    static time_t last_check;
    if (settings.shift_table_type != TABLE_NO_SHIFT && now.tv_sec != last_check)
    {
        check_shift(now.tv_sec);

        last_check = now.tv_sec;
    }

    static time_t last_log_min;
    time_t curr_min = now.tv_sec / 60;
    if (curr_min != last_log_min)
    {
        if (last_log_min != 0)
        {
            log_info("worker %d: insert succ: %d, fail: %d, other succ: %d, fail: %d", \
                    settings.worker_id, insert_db_succ_count, insert_db_fail_count, \
                    exec_sql_succ_count, exec_sql_fail_count);

            insert_db_succ_count = 0;
            insert_db_fail_count = 0;
            exec_sql_succ_count  = 0;
            exec_sql_fail_count  = 0;
        }

        last_log_min = curr_min;
    }
}

static void wait_for_notify(struct worker *worker)
{
    fd_set r_set;
    FD_ZERO(&r_set);
    FD_SET(worker->pipefd[0], &r_set);

    struct timeval timeout = { 0, 100 * 1000 };
    int ret = 0;

    ret = select(worker->pipefd[0] + 1, &r_set, NULL, NULL, &timeout);
    if (ret > 0 && FD_ISSET(worker->pipefd[0], &r_set))
    {
        char c;
        read(worker->pipefd[0], &c, 1);
    }

    return;
}

# define WORKER_BAD_CONN_USLEEP_TIME 100 * 1000

int do_worker_job(void)
{
    if (settings.shift_table_type == TABLE_NO_SHIFT)
    {
        create_new_tables();
    }

    struct worker *worker = &settings.workers[settings.worker_id];

    while (true)
    {
        worker_looper();

        char     *sql;
        uint32_t length;
        int      ret;
        bool     empty = false;

        ret = queue_pop(&worker->queue, (void **)&sql, &length);
        if (ret == -1)
        {
            empty = true;
        }
        else if (ret < 0)
        {
            log_error("queue_pop error: %d", ret);

            empty = true;
        }

        if (empty)
        {
            ret = queue_pop(&settings.cache_queue, (void **)&sql, &length);

            if (ret >= 0)
            {
                empty = false;
            }
            else if (ret < -1)
            {
                log_error("queue_pop error: %d", ret);
            }
        }

        if (empty)
        {
            wait_for_notify(worker);

            continue;
        }

        log_debug("worker: %d, sql: %s", settings.worker_id, sql);

        bool is_insert = true;
        if (strncmp(sql, "INSERT", 6) != 0)
            is_insert = false;

        ret = db_safe_query(sql, length - 1);

        if (ret < 0)
        {
            if (is_insert)
            {
                log_error("worker: %d, insert fail: %s", \
                        settings.worker_id, db_error());

                ++insert_db_fail_count;
            }
            else
            {
                log_error("worker: %d, exec sql: %s fail: %s", \
                        settings.worker_id, sql, db_error());

                ++exec_sql_fail_count;
            }

            if (ret < -1 || (ret == -1 && \
                        queue_push(&settings.cache_queue, sql, length) < 0))
            {
                if (is_insert)
                {
                    dlog(settings.fail_insert_log, "%s;", sql);
                }
            }

            if (ret == -1)
            {
                usleep(WORKER_BAD_CONN_USLEEP_TIME);
            }
        }
        else
        {
            if (is_insert)
            {
                int rows = db_affected_rows();
                insert_db_succ_count += rows;
            }
            else
            {
                exec_sql_succ_count += 1;
            }
        }
    }

    return 0;
}

