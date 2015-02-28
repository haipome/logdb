/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/17, create
 */

# pragma once

# include <stdbool.h>
# include <stdint.h>
# include <limits.h>
# include <sys/time.h>

# include "queue.h"
# include "dlog.h"

enum column_type
{
    COLUMN_TYPE_TINY_INT = 1,
    COLUMN_TYPE_SMALL_INT,
    COLUMN_TYPE_INT,
    COLUMN_TYPE_BIG_INT,
    COLUMN_TYPE_FLOAT,
    COLUMN_TYPE_DOUBLE,
    COLUMN_TYPE_CHAR,
    COLUMN_TYPE_VARCHAR,
    COLUMN_TYPE_TINY_TEXT,
    COLUMN_TYPE_TEXT,
    COLUMN_TYPE_BINARY,
    COLUMN_TYPE_VARBINARY,
    COLUMN_TYPE_TINY_BLOB,
    COLUMN_TYPE_BLOB,
    COLUMN_TYPE_DATE,
    COLUMN_TYPE_TIME,
    COLUMN_TYPE_DATETIME,
};

enum shift_type
{
    TABLE_SHIFT_BY_HOUR = 1,
    TABLE_SHIFT_BY_DAY,
    TABLE_SHIFT_BY_MON,
    TABLE_SHIFT_BY_YEAR,
    TABLE_NO_SHIFT,
};

enum alter_type
{
    ALTER_ADD = 1,
    ALTER_ADD_INDEX,
    ALTER_ADD_PRIMARY,
    ALTER_CHANGE,
    ALTER_CHANGE_LEN,
};

# define COLUMN_NAME_MAX_LEN 64

struct column
{
    char                *name;
    enum column_type    type;
    bool                is_unsigned;
    unsigned            length;

    bool                is_index;
    bool                is_primary;

    bool                is_auto_increment;
    bool                is_current_timestamp;
    bool                is_global_sequence;

    bool                is_const_length;        /* for string and bin */
    bool                is_zero_end;            /* for string */
    bool                is_unix_timestamp;      /* for time */

    bool                is_sender_ip;
    bool                is_sender_port;

    bool                is_zero;
    bool                is_storage;

    int                 alter_type;
    int                 alter_key_type;

    struct column       *next;
};

struct table
{
    char                *name;
    char                *buf;
    size_t              buf_len;
    size_t              buf_use;
    bool                not_first;
    struct timeval      start;
};

struct worker
{
    int                 pid;
    int                 pipefd[2];
    queue_t             queue;
};

struct settings
{
    char                *server_name;

    int                 worker_id;

    char                *local_ip;
    uint16_t            listen_port;

    char                *default_log_path;
    char                *default_log_flag;

    char                *fail_enqueue_log_path;
    dlog_t              *fail_enqueue_log;
    char                *fail_insert_log_path;
    dlog_t              *fail_insert_log;

    int                 worker_proc_num;
    struct worker       *workers;

    int                 queue_base_shm_key;
    uint32_t            queue_mem_cache_size;
    char                *queue_bin_file_path;
    uint64_t            queue_bin_file_max_size;

    queue_t             cache_queue;

    struct column       *columns;
    char                *columns_str;
    size_t              columns_str_len;

    uint16_t             db_port;
    char                *db_host;
    char                *db_name;
    char                *db_user;
    char                *db_passwd;
    char                *db_table_name;
    char                *db_merge_table_name;
    char                *db_charset;
    char                *db_engine;

    bool                is_utf8;

    int                 cache_len;
    int                 cache_time_in_ms;
    int                 check_time_in_ms;

    int                 shift_table_type;
    int                 hash_table_num;
    struct column       *hash_table_column;
    struct table        *tables;
    unsigned            data_keep_time;

    bool                has_global_sequence;
    char                *global_sequence_file;

    char                *api_head_path;
    char                *api_source_path;
};

extern struct settings settings;

int nametotype(char const *name);

int read_settings(char const *config_file_path);

bool is_local_generate(struct column *curr);

