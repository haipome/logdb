/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/18, create
 */


# pragma once

# include "conf.h"

char *db_error(void);

int db_connect(void);

int db_query(const void *query, size_t length);

int db_safe_query(const void *query, size_t length);

int db_affected_rows(void);

int db_escape_string(char *to, const char *from, size_t len);

void db_close(void);

int db_desc_table(char *table, int *num, struct column **columns);
int db_show_create_table(char *table, char **result);

