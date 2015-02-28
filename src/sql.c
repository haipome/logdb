/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/02/24, create
 */


# include <stdio.h>
# include <string.h>
# include <math.h>
# include <assert.h>

# include "conf.h"
# include "utils.h"
# include "db.h"

static char *strreplace(char *des, size_t n, const char *from, const char *to)
{
    char *s = strstr(des, from);
    if (s == NULL)
        return NULL;

    char *end = strdup(s + strlen(from));
    if (end == NULL)
        return NULL;

    *s = '\0';

    strncat(des, to, n);
    strncat(des, end, n);

    free(end);

    return des;
}

char *get_table_name(int table_id, int offset)
{
    if (table_id == -1 && settings.db_merge_table_name)
        return settings.db_merge_table_name;

    static char *name = NULL;
    static size_t len = 0;
    size_t base_name_len = 0;

    if (name == NULL)
    {
        len = strlen(settings.db_table_name) + 30;
        name = malloc(len);
        if (name == NULL)
            return NULL;
    }

    base_name_len = strlen(strcpy(name, settings.db_table_name));

    char *time_str = NULL;
    switch (settings.shift_table_type)
    {
    case TABLE_SHIFT_BY_HOUR:
        time_str = get_hour_str(offset, NULL);
        break;
    case TABLE_SHIFT_BY_DAY:
        time_str = get_day_str(offset, NULL);
        break;
    case TABLE_SHIFT_BY_MON:
        time_str = get_mon_str(offset, NULL);
        break;
    case TABLE_SHIFT_BY_YEAR:
        time_str = get_year_str(offset, NULL);
        break;
    case TABLE_NO_SHIFT:
        time_str = NULL;
        break;
    default:
        log_fatal("unknow db table shift type: %d", settings.shift_table_type);

        return NULL;
    }

    if (time_str)
    {
        if (strreplace(name, len, "{time}", time_str) == NULL)
            snprintf(name, len, "%s_%s", settings.db_table_name, time_str);
        base_name_len = strlen(name);
    }

    if (settings.hash_table_num > 1)
    {
        char id_str[16];

        if (table_id == -1)
        {
            snprintf(id_str, sizeof(id_str), "merge");
        }
        else
        {
            snprintf(id_str, sizeof(id_str), "%d", table_id);
        }

        if (strreplace(name, len, "{hash}", id_str) == NULL)
            snprintf(name + base_name_len, len - base_name_len, "_%s", id_str);
    }

    return name;
}

static char *sql;
static size_t sql_buf_len;

static const char *column_type_names[] =
{
    "",
    "TINYINT",
    "SMALLINT",
    "INT",
    "BIGINT",
    "FLOAT",
    "DOUBLE",
    "CHAR",
    "VARCHAR",
    "TINYTEXT",
    "TEXT",
    "BINARY",
    "VARBINARY",
    "TINYBLOB",
    "BLOB",
    "DATE",
    "TIME",
    "DATETIME",
};

static int get_column_def(char *des, size_t n, struct column *target)
{
    assert(target->type >= COLUMN_TYPE_TINY_INT && target->type <= COLUMN_TYPE_DATETIME);

    int len = 0;
    char name[strlen(target->name) + 3];
    sprintf(name, "`%s`", target->name);
    len += snprintf(des, n, "%-10s %s", name, column_type_names[target->type]);

    if (target->type >= COLUMN_TYPE_TINY_INT && target->type <= COLUMN_TYPE_BIG_INT)
    {
        if (target->is_unsigned)
        {
            len += snprintf(des + len, n - len, " UNSIGNED");
        }
    }

    if (target->type == COLUMN_TYPE_CHAR || target->type == COLUMN_TYPE_VARCHAR || \
            target->type == COLUMN_TYPE_BINARY || target->type == COLUMN_TYPE_VARBINARY)
    {
        len += snprintf(des + len, n - len, "(%u)", target->length);
    }

    if (target->type >= COLUMN_TYPE_TINY_INT && target->type <= COLUMN_TYPE_BIG_INT)
    {
        if (target->is_auto_increment == false)
            len += snprintf(des + len, n - len, " NOT NULL DEFAULT 0");
        else
            len += snprintf(des + len, n - len, " NOT NULL AUTO_INCREMENT PRIMARY KEY");
    }
    else if (target->type == COLUMN_TYPE_FLOAT || target->type == COLUMN_TYPE_DOUBLE)
    {
        len += snprintf(des + len, n - len, " NOT NULL DEFAULT 0");
    }
    else if (target->type == COLUMN_TYPE_CHAR || target->type == COLUMN_TYPE_VARCHAR || \
            target->type == COLUMN_TYPE_BINARY || target->type == COLUMN_TYPE_VARBINARY)
    {
        len += snprintf(des + len, n - len, " NOT NULL DEFAULT ''");
    }
    else if (target->type >= COLUMN_TYPE_DATE && target->type <= COLUMN_TYPE_DATETIME)
    {
        len += snprintf(des + len, n - len, " NOT NULL DEFAULT 0");
    }
    /* TEXT and BLOB don't have default value */

    return len;
}

static char *get_table_def(void)
{
    static char *buf;
    static size_t buf_len;

    size_t len = 0;

    int index_num = 0;
    struct column *curr = settings.columns;
    while (curr)
    {
        if (curr->is_storage == false)
        {
            curr = curr->next;
            continue;
        }

        if (auto_realloc((void **)&buf, &buf_len, len + strlen(curr->name) + 256) == NULL)
            return NULL;

        len += lstrncpy(buf + len, "        ", buf_len - len);
        len += get_column_def(buf + len, buf_len - len, curr);
        len += snprintf(buf + len, buf_len - len, ",\n");

        if (curr->is_index)
            index_num += 1;

        curr = curr->next;
    }

    len -= 2; /* delete the last ,\n */

    curr = settings.columns;
    while (curr)
    {
        if (auto_realloc((void **)&buf, &buf_len, len + strlen(curr->name) + 256) == NULL)
            return NULL;

        if (curr->is_index)
        {
            len += snprintf(buf + len, buf_len - len, ",\n        INDEX `index_%s` (`%s`)", \
                    curr->name, curr->name);
        }
        else if (curr->is_primary && curr->is_auto_increment == false)
        {
            len += snprintf(buf + len, buf_len -len, ", \n        PRIMARY KEY (`%s`)", \
                    curr->name);
        }

        curr = curr->next;
    }

    len += snprintf(buf + len, buf_len - len, "\n");

    return buf;
}

char *get_table_def_by_table(char *table)
{
    char *create_table = NULL;
    int ret = db_show_create_table(table, &create_table);
    if (ret < 0)
        return NULL;

    char *start = strstr(create_table, "(");
    char *end   = strstr(create_table, ") ENGINE=");
    if (start == NULL || end == NULL)
    {
        free(create_table);
        return NULL;
    }

    *end = 0;
    start++;
    if (*start == '\n')
        start++;
    char *def = strdup(start);
    free(create_table);

    return def;
}

char *create_merge_table_sql(int offset)
{
    if (strcasecmp(settings.db_engine, "myisam") != 0)
        return NULL;

# define FMT_CREATE "CREATE TABLE IF NOT EXISTS `%s` (\n"
    char *merge_table = get_table_name(-1, offset);
    if (merge_table == NULL)
        return NULL;
    if (auto_realloc((void **)&sql, &sql_buf_len, strlen(FMT_CREATE) + \
                strlen(merge_table)) == NULL)
        return NULL;

    size_t len = 0;
    len += snprintf(sql, sql_buf_len, FMT_CREATE, merge_table);
# undef FMT_CREATE

    char *normal_table = get_table_name(settings.hash_table_num - 1, offset);
    if (normal_table == NULL)
        return NULL;
    char *create_table = get_table_def_by_table(normal_table);
    if (create_table == NULL)
        return NULL;

    if (auto_realloc((void **)&sql, &sql_buf_len, len + strlen(create_table) + 100) == NULL)
        return NULL;

    len += lstrncpy(sql + len, create_table, sql_buf_len - len);
    len += lstrncpy(sql + len, ") ENGINE=MERGE UNION=(\n", sql_buf_len - len);

    free(create_table);

    int i;
    for (i = 0; i < settings.hash_table_num; ++i)
    {
        char *table_name = get_table_name(i, offset);
        if (table_name == NULL)
            return NULL;

        if (auto_realloc((void **)&sql, &sql_buf_len, len + strlen(table_name) + 100) == NULL)
            return NULL;

        len += lstrncpy(sql + len, "        `", sql_buf_len - len);
        len += lstrncpy(sql + len, table_name, sql_buf_len - len);
        len += lstrncpy(sql + len, "`", sql_buf_len - len);
        if (i != settings.hash_table_num - 1)
            len += lstrncpy(sql + len, ",\n", sql_buf_len - len);
        else
            len += lstrncpy(sql + len, "\n", sql_buf_len - len);
    }

    len += snprintf(sql + len, sql_buf_len - len, ") DEFAULT CHARSET=%s", settings.db_charset);

    return sql;
}

char *create_table_sql(char *table_name)
{
# define CREATE_TABLE "CREATE TABLE IF NOT EXISTS `%s` (\n"
    if (auto_realloc((void **)&sql, &sql_buf_len, strlen(CREATE_TABLE) + strlen(table_name)) == NULL)
        return NULL;

    size_t len = 0;
    len += snprintf(sql, sql_buf_len, CREATE_TABLE, table_name);
# undef CREATE_TABLE

    char *create_table = get_table_def();
    if (create_table == NULL)
        return NULL;

    if (auto_realloc((void **)&sql, &sql_buf_len, len + strlen(create_table) + 100) == NULL)
        return NULL;

    len += lstrncpy(sql + len, create_table, sql_buf_len - len);

    len += snprintf(sql + len, sql_buf_len - len, ") ENGINE=%s DEFAULT CHARSET=%s", \
            settings.db_engine, settings.db_charset);

    return sql;
}

char *add_column_sql(char *table, struct column *target)
{
    if (auto_realloc((void **)&sql, &sql_buf_len, 1024) == NULL)
        return NULL;

    size_t len = 0;
    len += snprintf(sql + len, sql_buf_len - len, "ALTER TABLE `%s` ADD ", table);

    if (get_column_def(sql + len, sql_buf_len - len, target) < 0)
        return NULL;

    return sql;
}

char *change_column_sql(char *table, struct column *target)
{
    if (auto_realloc((void **)&sql, &sql_buf_len, 1024) == NULL)
        return NULL;

    size_t len = 0;
    len += snprintf(sql + len, sql_buf_len - len, "ALTER TABLE `%s` CHANGE `%s` ", \
            table, target->name);

    if (get_column_def(sql + len, sql_buf_len - len, target) < 0)
        return NULL;

    return sql;
}

