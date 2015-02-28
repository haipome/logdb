/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/18, create
 */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include <mysql.h>
# include <errmsg.h>
# include <mysqld_error.h>

# include "conf.h"
# include "dlog.h"
# include "db.h"
# include "utils.h"
# include "utf8.h"

static MYSQL *mysql_conn;
static bool connect_flag;

char *db_error(void)
{
    return (char *)mysql_error(mysql_conn);
}

int db_connect(void)
{
    if (connect_flag == true)
        mysql_close(mysql_conn);

    mysql_conn = mysql_init(NULL);
    if (mysql_conn == NULL)
        return -__LINE__;
    connect_flag = true;

    /* set mysql auto reconnect */
    my_bool reconnect = 1;
    if (mysql_options(mysql_conn, MYSQL_OPT_RECONNECT, &reconnect) != 0)
        return -__LINE__;

    /* set charst is important for: mysql_real_escape_string
     * also can use mysql_set_character_set on mysql 5.0.7 or higher */
    if (mysql_options(mysql_conn, MYSQL_SET_CHARSET_NAME, settings.db_charset) != 0)
        return -__LINE__;

    if (mysql_real_connect(mysql_conn, settings.db_host, settings.db_user, \
                settings.db_passwd, settings.db_name, settings.db_port, NULL, 0) == NULL)
    {
        return -__LINE__;
    }

    return 0;
}

int db_query(const void *query, size_t length)
{
    if (connect_flag == 0)
    {
        if (db_connect() < 0)
            return -__LINE__;
    }

    if (length == 0)
        length = strlen(query);

    int ret = mysql_real_query(mysql_conn, query, (unsigned long)length);
    if (ret != 0)
    {
        unsigned errcode = mysql_errno(mysql_conn);
        log_notice("mysql errno: %u", errcode);

        if (
                errcode == CR_SERVER_LOST       ||  /* 2013 */
                errcode == CR_SERVER_GONE_ERROR ||  /* 2006 */
                errcode == CR_CONNECTION_ERROR  ||  /* 2002 */
                errcode == CR_CONN_HOST_ERROR   ||  /* 2003 */
                errcode == ER_SERVER_SHUTDOWN   ||  /* 1053 */
                errcode == ER_QUERY_INTERRUPTED)    /* 1317 */

        {
            log_notice("mysql connect lost");

            return -1;
        }
        else
        {
            return -__LINE__;
        }
    }

    return 0;
}

int db_safe_query(const void *query, size_t length)
{
    NEG_RET(db_query(query, length));

    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (result != NULL)
        mysql_free_result(result);

    return 0;
}

int db_affected_rows(void)
{
    return (int)mysql_affected_rows(mysql_conn);
}

int db_escape_string(char *to, const char *from, size_t len)
{
    if (settings.is_utf8)
    {
        /*
         * Mysql utf8 only support one to three bytes per character.
         * That is no more than 0xffff in unicode.
         * To support four bytes utf8 character, you need use uft8mb4
         * charater set and newer Mysql version.
         *
         * http://dev.mysql.com/doc/refman/5.7/en/charset-unicode-sets.html
         */
        ucs4_t us[len + 1];
        int    illegal = 0;
        size_t n = 0;

        n = u8decode((char *)from, us, len + 1, &illegal);
        if (illegal != 0)
            log_warn("illegal utf8 string: %s, illegal length: %d", from, illegal);

        size_t pos = 0;
        size_t unsupported = 0;
        size_t i;
        for (i = 0; i < n; ++i)
        {
            if (us[i] > 0xffff)
                ++unsupported;
            else
                us[pos++] = us[i];
        }
        us[pos] = 0;

        if (unsupported != 0)
        {
            log_warn("string: %s, %zu unsupported utf8 chars are deleted", \
                    from, unsupported);
        }

        char str[len + 1];
        n = u8encode(us, str, len + 1, NULL);

        return mysql_real_escape_string(mysql_conn, to, str, (unsigned long)n);
    }

    return mysql_real_escape_string(mysql_conn, to, from, (unsigned long)len);
}

void db_close(void)
{
    if (connect_flag == true)
        mysql_close(mysql_conn);

    connect_flag = false;
}

static int get_field_id(int num_fields, MYSQL_FIELD *fields, const char *name)
{
    int i;

    for (i = 0; i < num_fields; ++i)
    {
        if (strcmp(fields[i].name, name) == 0)
            return i;
    }

    return -1;
}

static int get_type_by_str(struct column *res, char *type)
{
    char *s = strdup(type);
    if (s == NULL)
        return -__LINE__;

    char *p = strtok(s, "() ");
    if (!p)
    {
        free(s);
        return -__LINE__;
    }

    res->type = nametotype(p);

    p = strtok(NULL, "() ");
    if (!p)
    {
        free(s);
        return 0;
    }

    if (res->type == COLUMN_TYPE_CHAR || res->type == COLUMN_TYPE_VARCHAR || \
            res->type == COLUMN_TYPE_BINARY || res->type == COLUMN_TYPE_VARBINARY)
    {
        res->length = (unsigned)atoi(p);
    }

    p = strtok(NULL, "() ");
    if (!p)
    {
        free(s);
        return 0;
    }

    if (res->type >= COLUMN_TYPE_TINY_INT && res->type <= COLUMN_TYPE_BIG_INT)
    {
        if (strcmp(p, "unsigned") == 0)
            res->is_unsigned = true;
    }

    free(s);

    return 0;
}

int db_desc_table(char *table, int *num, struct column **columns)
{
    char sql[10 + strlen(table)];
    sprintf(sql, "DESC `%s`", table);

    int ret = db_query(sql, 0);
    if (ret < 0)
    {
        if (strstr(db_error(), "doesn't exist"))
            return -1;

        return -__LINE__;
    }

    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (result == NULL)
        return -__LINE__;

    unsigned num_fields = mysql_num_fields(result);
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    int type_id  = 0;
    int key_id   = 0;
    int extra_id = 0;

    NEG_RET_LN(type_id  = get_field_id(num_fields, fields, "Type"));
    NEG_RET_LN(key_id   = get_field_id(num_fields, fields, "Key"));
    NEG_RET_LN(extra_id = get_field_id(num_fields, fields, "Extra"));

    int row_num = (int)mysql_num_rows(result);
    struct column *res = calloc(row_num, sizeof(struct column));
    if (res == NULL)
        return -__LINE__;

    int i;
    for (i = 0; i < row_num; ++i)
    {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row == NULL)
        {
            mysql_free_result(result);
            return -__LINE__;
        }

        res[i].name = strdup(row[0]);
        if (res[i].name == NULL)
            return -__LINE__;

        if (strcmp(row[key_id], "MUL") == 0)
            res[i].is_index = true;
        else if (strcmp(row[key_id], "PRI") == 0)
            res[i].is_primary = true;

        NEG_RET_LN(get_type_by_str(&res[i], row[type_id]));

        if (strcmp(row[extra_id], "auto_increment") == 0)
            res[i].is_auto_increment = true;
    }

    mysql_free_result(result);

    *num = row_num;
    *columns = res;

    return 0;
}

int db_show_create_table(char *table, char **create_tabel)
{
    char sql[30 + strlen(table)];
    sprintf(sql, "SHOW CREATE TABLE `%s`", table);

    int ret = db_query(sql, 0);
    if (ret < 0)
        return -__LINE__;

    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (result == NULL)
        return -__LINE__;

    int row_num = (int)mysql_num_rows(result);
    if (row_num != 1)
    {
        mysql_free_result(result);
        return -__LINE__;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    *create_tabel = strdup(row[1]);

    mysql_free_result(result);

    return 0;
}

