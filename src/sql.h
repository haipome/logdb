/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/02/24, create
 */

# pragma once

char *get_table_name(int table_id, int offset);

char *create_table_sql(char *table_name);

char *create_merge_table_sql(int offset);

char *add_column_sql(char *table, struct column *target);

char *change_column_sql(char *table, struct column *target);

