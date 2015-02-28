/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/17, create
 */


# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <limits.h>

# include "conf.h"
# include "ini.h"
# include "utils.h"

struct settings settings;

static bool is_legal_column_name(char *name)
{
    size_t len = strlen(name);

    if (len > COLUMN_NAME_MAX_LEN)
    {
        fprintf(stderr, "column name: '%s' too long, max length: %d\n", \
                name, COLUMN_NAME_MAX_LEN);

        return false;
    }

    size_t i;
    for (i = 0; i < len; ++i)
    {
        if (i == 0)
        {
            if (!(isalpha(name[i]) || name[i] == '_'))
                return false;
        }
        else
        {
            if (!(isalnum(name[i]) || name[i] == '_'))
                return false;
        }
    }

    return true;
}

static struct column *find_column(struct column *columns, char *name)
{
    struct column *curr = columns;
    while (curr)
    {
        if (strcmp(curr->name, name) == 0)
            return curr;

        curr = curr->next;
    }

    return NULL;
}

int nametotype(char const *name)
{
    static const char *type_names[] =
    {
        "",
        "tinyint",
        "smallint",
        "int",
        "bigint",
        "float",
        "double",
        "char",
        "varchar",
        "tinytext",
        "text",
        "binary",
        "varbinary",
        "tinyblob",
        "blob",
        "date",
        "time",
        "datetime",
    };

    int i;
    for (i = COLUMN_TYPE_TINY_INT; i <= COLUMN_TYPE_DATETIME; ++i)
    {
        if (strcmp(name, type_names[i]) == 0)
            return i;
    }

    return 0;
}

static int read_columns(ini_t *conf)
{
    char *columns_string = NULL;
    if (ini_read_str(conf, "", "columns", &columns_string, NULL) != 0)
    {
        fprintf(stderr, "'columns' is required field\n");

        return -__LINE__;
    }

    struct column *prev_column = NULL;
    struct column *curr_column = NULL;

    int primary_key_num = 0;
    int storage_num = 0;

    char *column = strtok(columns_string, "\t\r ,");
    while (column != NULL)
    {
        if (find_column(settings.columns, column) != NULL)
        {
            fprintf(stderr, "column %s has exists\n", column);

            return -__LINE__;
        }

        if ((curr_column = calloc(1, sizeof(struct column))) == NULL)
            return -__LINE__;

        if (settings.columns == NULL)
            settings.columns = curr_column;
        if (prev_column)
            prev_column->next = curr_column;
        prev_column = curr_column;

        if ((curr_column->name = strdup(column)) == NULL)
            return -__LINE__;

        if (is_legal_column_name(curr_column->name) == false)
        {
            fprintf(stderr, "column name: %s is illegal\n", column);

            return -__LINE__;
        }

        char *type = NULL;
        if (ini_read_str(conf, column, "type", &type, NULL) != 0)
        {
            fprintf(stderr, "in column %s, 'type' is required field\n", column);

            return -__LINE__;
        }

        strtolower(type);

        char *ctype = NULL;
        char *part1 = type;
        char *part2 = part1;

        while (*part2 && !isspace(*part2))
            ++part2;

        if (*part2 == '\0')
        {
            part2 = NULL;
        }
        else
        {
            while (isspace(*part2))
                *part2++ = '\0';
        }

        if (part1 && part2)
        {
            ctype = part2;

            if (strcmp(part1, "unsigned") == 0)
            {
                curr_column->is_unsigned = true;
            }
            else if (strcmp(part1, "signed") == 0)
            {
                curr_column->is_unsigned = false;
            }
            else
            {
                fprintf(stderr, "in column: %s, unknown 'type': %s\n", column, type);

                return -__LINE__;
            }
        }
        else if (part1)
        {
            ctype = part1;
        }
        else
        {
            fprintf(stderr, "in column %s, 'type' is required field\n", column);

            return -__LINE__;
        }

        if ((curr_column->type = nametotype(ctype)) == 0)
        {
            fprintf(stderr, "in column %s, unknown 'type': %s\n", column, ctype);

            return -__LINE__;
        }

        if (curr_column->type >= COLUMN_TYPE_FLOAT && curr_column->is_unsigned == true)
        {
            fprintf(stderr, "warn: in column %s, unsigned is not suitable for %s", \
                    column, ctype);

            curr_column->is_unsigned = false;
        }

        free(type);

        if (curr_column->type >= COLUMN_TYPE_TINY_INT && curr_column->type <= COLUMN_TYPE_BIG_INT)
        {
            if (ini_read_bool(conf, column, "auto increment", &curr_column->is_auto_increment, false) < 0)
                return -__LINE__;
            if (curr_column->is_auto_increment)
                curr_column->is_primary = true;

            if (ini_read_bool(conf, column, "global sequence", &curr_column->is_global_sequence, false) < 0)
                return -__LINE__;
            if (curr_column->is_global_sequence)
                settings.has_global_sequence = true;

            if (ini_read_bool(conf, column, "sender ip", &curr_column->is_sender_ip, false) < 0)
                return -__LINE__;

            if (ini_read_bool(conf, column, "sender port", &curr_column->is_sender_port, false) < 0)
                return -__LINE__;
        }

        if (curr_column->type >= COLUMN_TYPE_CHAR && curr_column->type <= COLUMN_TYPE_BLOB)
        {
            if (ini_read_unsigned(conf, column, "length", &curr_column->length, 0) != 0)
            {
                fprintf(stderr, "in column: %s, length is required field\n", column);

                return -__LINE__;
            }

            if (curr_column->type == COLUMN_TYPE_CHAR || curr_column->type == COLUMN_TYPE_TINY_TEXT || \
                    curr_column->type == COLUMN_TYPE_BINARY || curr_column->type == COLUMN_TYPE_TINY_BLOB)
            {
                if (curr_column->length > UINT8_MAX)
                {
                    fprintf(stderr, "column: %s length: %u shoule be less than: %u\n", \
                            curr_column->name, curr_column->length, UINT8_MAX);
                    return -__LINE__;
                }
            }
            else
            {
                if (curr_column->length > UINT16_MAX)
                {
                    fprintf(stderr, "column: %s length: %u shoule be less than: %u\n", \
                            curr_column->name, curr_column->length, UINT16_MAX);
                    return -__LINE__;
                }
            }

            if (ini_read_bool(conf, column, "const length", &curr_column->is_const_length, false) < 0)
                return -__LINE__;
        }

        if (curr_column->type >= COLUMN_TYPE_CHAR && curr_column->type <= COLUMN_TYPE_TEXT)
        {
            if (curr_column->is_const_length == false)
            {
                if (ini_read_bool(conf, column, "zero end", &curr_column->is_zero_end, false) < 0)
                    return -__LINE__;
            }
        }

        if ((curr_column->type >= COLUMN_TYPE_TINY_INT && curr_column->type <= COLUMN_TYPE_BIG_INT) || \
                (curr_column->type >= COLUMN_TYPE_DATE && curr_column->type <= COLUMN_TYPE_DATETIME))
        {
            if (ini_read_bool(conf, column, "current timestamp", &curr_column->is_current_timestamp, false) < 0)
                return -__LINE__;
        }

        if (curr_column->type >= COLUMN_TYPE_DATE && curr_column->type <= COLUMN_TYPE_DATETIME)
        {
            if (ini_read_bool(conf, column, "unix timestamp", &curr_column->is_unix_timestamp, false) < 0)
                return -__LINE__;
        }

        if (ini_read_bool(conf, column, "zero", &curr_column->is_zero, false) < 0)
            return -__LINE__;

        if (ini_read_bool(conf, column, "storage", &curr_column->is_storage, true) < 0)
            return -__LINE__;
        if (curr_column->is_storage)
            ++storage_num;

        if (ini_read_bool(conf, column, "index", &curr_column->is_index, false) < 0)
            return -__LINE__;

        if (ini_read_bool(conf, column, "primary", &curr_column->is_primary, false) < 0)
            return -__LINE__;

        if (curr_column->is_primary)
        {
            primary_key_num += 1;
            if (primary_key_num > 1)
            {
                fprintf(stderr, "only can define one 'primary' or 'auto increment'\n");
                return -__LINE__;
            }
        }

        column = strtok(NULL, "\t ,");
    }

    free(columns_string);

    if (storage_num == 0)
    {
        fprintf(stderr, "nothing to storage\n");

        return -__LINE__;
    }

    size_t columns_str_n = 0;
    curr_column = settings.columns;
    while (curr_column)
    {
        if (curr_column->is_storage == false)
        {
            curr_column = curr_column->next;
            continue;
        }

        settings.columns_str = auto_realloc((void **)&settings.columns_str,\
                &columns_str_n, settings.columns_str_len + strlen(curr_column->name) + 3);
        if (settings.columns_str == NULL)
            return -__LINE__;
    
        if (settings.columns_str_len == 0)
        {
            settings.columns_str[0] = '\0';
        }
        else
        {
            strcat(settings.columns_str, ",");
        }

        strcat(settings.columns_str, "`");
        strcat(settings.columns_str, curr_column->name);
        strcat(settings.columns_str, "`");

        settings.columns_str_len = strlen(settings.columns_str);

        curr_column = curr_column->next;
    }

    return 0;
}

static int read_shift_table_type(ini_t *conf)
{
    char *shift_table_type_str = NULL;
    if (ini_read_str(conf, "", "db shift table type", &shift_table_type_str, "no") < 0)
        return -__LINE__;

    strtolower(shift_table_type_str);

    if (strcmp(shift_table_type_str, "hour") == 0)
    {
        settings.shift_table_type = TABLE_SHIFT_BY_HOUR;
    }
    else if (strcmp(shift_table_type_str, "day") == 0)
    {
        settings.shift_table_type = TABLE_SHIFT_BY_DAY;
    }
    else if (strcmp(shift_table_type_str, "month") == 0)
    {
        settings.shift_table_type = TABLE_SHIFT_BY_MON;
    }
    else if (strcmp(shift_table_type_str, "mon") == 0)
    {
        settings.shift_table_type = TABLE_SHIFT_BY_MON;
    }
    else if (strcmp(shift_table_type_str, "year") == 0)
    {
        settings.shift_table_type = TABLE_SHIFT_BY_YEAR;
    }
    else if (strcmp(shift_table_type_str, "no") == 0)
    {
        settings.shift_table_type = TABLE_NO_SHIFT;
    }
    else
    {
        fprintf(stderr, "shift table type should be one of: hour, day, month, year or no.\n");

        return -__LINE__;
    }

    free(shift_table_type_str);

    return 0;
}

int read_settings(char const *config_file_path)
{
    ini_t *conf = ini_load(config_file_path);
    if (conf == NULL)
        return -__LINE__;

    if (ini_read_str(conf, "", "server name", &settings.server_name, NULL) != 0)
    {
        fprintf(stderr, "'server name' is required field\n");

        return -__LINE__;
    }

    char *server_name = settings.server_name;
    while (*server_name && !isspace(*server_name))
        ++server_name;
    *server_name = '\0';

    if (ini_read_str(conf, "", "local ip", &settings.local_ip, NULL) < 0)
        return -__LINE__;

    if (ini_read_uint16(conf, "", "listen port", &settings.listen_port, 22060) < 0)
        return -__LINE__;
    settings.listen_port += 1;

    if (ini_read_int(conf, "", "cache time in ms", &settings.cache_time_in_ms, 500) < 0)
        return -__LINE__;
    settings.check_time_in_ms = (settings.cache_time_in_ms + 4) / 5;
    if (settings.check_time_in_ms > 100)
        settings.check_time_in_ms = 100;

    if (ini_read_int(conf, "", "cache size", &settings.cache_len, 1000000) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "default log path", \
                &settings.default_log_path, "../log/default") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "default log flag", \
                &settings.default_log_flag, "fatal, error, warn, info, notice") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "fail enqueue log path", \
                &settings.fail_enqueue_log_path, "../log/enqueue_fail") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "fail insert log path", \
                &settings.fail_insert_log_path, "../log/insert_fail") < 0)
        return -__LINE__;

    if (ini_read_int(conf, "", "worker process num", \
                &settings.worker_proc_num, 1) < 0)
        return -__LINE__;

    if (ini_read_int(conf, "", "queue base shm key", \
                &settings.queue_base_shm_key, 0) != 0)
    {
        fprintf(stderr, "'queue base shm key' is required field\n");

        return -__LINE__;
    }

    if (settings.queue_base_shm_key == 0)
    {
        fprintf(stderr, "'queue base shm key' must be a integer\njust type a random number\n");

        return -__LINE__;
    }

    if (ini_read_uint32(conf, "", "queue memory cache size", \
                &settings.queue_mem_cache_size, 8 * 1024 * 1024) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "queue bin file path", \
                &settings.queue_bin_file_path, "../binlog/queue") < 0)
        return -__LINE__;

    if (ini_read_uint64(conf, "", "queue bin file max size", \
                &settings.queue_bin_file_max_size, 10 * 1024 * 1024 * 1024ull) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "global sequence file", \
                &settings.global_sequence_file, "../binlog/global_sequence") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "db host", &settings.db_host, "localhost") < 0)
        return -__LINE__;

    if (ini_read_uint16(conf, "", "db port", &settings.db_port, 3306) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "db name", &settings.db_name, NULL) != 0)
    {
        fprintf(stderr, "'db name' is required field\n");

        return -__LINE__;
    }

    if (ini_read_str(conf, "", "db user", &settings.db_user, "root") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "db passwd", &settings.db_passwd, NULL) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "db charset", &settings.db_charset, "utf8") < 0)
        return -__LINE__;

    if (strcmp(settings.db_charset, "utf8") == 0)
        settings.is_utf8 = true;

    if (ini_read_str(conf, "", "db engine", &settings.db_engine, "MyISAM") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "db table name", &settings.db_table_name, NULL) != 0)
    {
        fprintf(stderr, "'db table name' is required field\n");

        return -__LINE__;
    }

    if (ini_read_str(conf, "", "db merge table name", &settings.db_merge_table_name, NULL) < 0)
        return -__LINE__;

    NEG_RET(read_shift_table_type(conf));

    if (ini_read_unsigned(conf, "", "db keep time", &settings.data_keep_time, 0) < 0)
        return -__LINE__;

    NEG_RET(read_columns(conf));

    if (ini_read_int(conf, "", "hash table num", &settings.hash_table_num, 1) < 0)
        return -__LINE__;

    if (settings.hash_table_num <= 0)
        settings.hash_table_num = 1;

    if (settings.hash_table_num > 1)
    {
        char *hash_table_column_name = NULL;
        if (ini_read_str(conf, "", "hash table column", &hash_table_column_name, NULL) != 0)
        {
            fprintf(stderr, "'hash table column' is required field\n");

            return -__LINE__;
        }

        settings.hash_table_column = find_column(settings.columns, hash_table_column_name);
        if (settings.hash_table_column == NULL)
        {
            fprintf(stderr, "field 'hash table column': %s not found in columns\n", \
                    hash_table_column_name);

            return -__LINE__;
        }

        if (settings.hash_table_column->type >= COLUMN_TYPE_BINARY)
        {
            fprintf(stderr, "field 'hash table column' type must be integer or string\n");

            return -__LINE__;
        }

        if (
                settings.hash_table_column->is_auto_increment       ||
                settings.hash_table_column->is_global_sequence      ||
                settings.hash_table_column->is_current_timestamp    ||
                settings.hash_table_column->is_sender_ip            ||
                settings.hash_table_column->is_sender_port          ||
                settings.hash_table_column->is_zero)
        {
            fprintf(stderr, "field 'hash table column' should not be 'auto increment' or "
                    "'global sequence' or 'current timestamp' or "
                    "'sender ip' or 'sender port' or 'zero'\n");

            return -__LINE__;
        }

        free(hash_table_column_name);
    }

    if ((settings.tables = calloc(settings.hash_table_num, sizeof(struct table))) == NULL)
        return -__LINE__;

    if (ini_read_str(conf, "", "api head file", &settings.api_head_path, NULL) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "api source file", &settings.api_source_path, NULL) < 0)
        return -__LINE__;

    if (settings.api_head_path == NULL)
    {
        settings.api_head_path = malloc(strlen(settings.server_name) + 20);
        if (settings.api_head_path == NULL)
            return -__LINE__;

        sprintf(settings.api_head_path, "../api/log_%s_api.h", settings.server_name);
    }

    if (settings.api_source_path == NULL)
    {
        settings.api_source_path = malloc(strlen(settings.server_name) + 20);
        if (settings.api_source_path == NULL)
            return -__LINE__;

        sprintf(settings.api_source_path, "../api/log_%s_api.c", settings.server_name);
    }

    ini_free(conf);

    return 0;
}

bool is_local_generate(struct column *curr)
{
    if (curr->is_auto_increment || curr->is_current_timestamp || \
            curr->is_sender_ip  || curr->is_sender_port       || \
            curr->is_zero       || curr->is_global_sequence)
    {
        return true;
    }

    return false;
}

