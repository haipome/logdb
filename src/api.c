/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/25, create
 */


# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <assert.h>

# include "conf.h"
# include "utils.h"

static char packf_fmt[4096];
static FILE *fpc;
static FILE *fph;

# define ph(fmt, args...) fprintf(fph, fmt "\n", ##args)
# define pc(fmt, args...) fprintf(fpc, fmt "\n", ##args)

static int generate_h_api_h(void)
{
    ph("# pragma once");
    ph();
    ph("# include <stdint.h>");
    ph("# include <stdarg.h>");
    ph("# include <netinet/in.h>");
    ph();
    ph("# pragma pack(1)");
    ph();
    ph("typedef struct");
    ph("{");

    static char const *types_str[] =
    {
        "",
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "float",
        "double",
        "char",
        "char",
        "char",
        "char",
        "uint8_t",
        "uint8_t",
        "uint8_t",
        "uint8_t",
        "char",
        "char",
        "char",
    };

    static char const *packf_str[] =
    {
        "",
        "c",
        "w",
        "d",
        "D",
        "f",
        "F",
    };

    memset(packf_fmt, 0, sizeof(packf_fmt));
    strcat(packf_fmt, "[");

    size_t line_num = 0;
    struct column *curr = settings.columns;
    while (curr)
    {
        if (is_local_generate(curr))
        {
            curr = curr->next;
            continue;
        }

        assert(curr->type >= COLUMN_TYPE_TINY_INT && curr->type <= COLUMN_TYPE_DATETIME);

        char type[20] = { 0 };
        const char *len_type = NULL;

        if (curr->is_unix_timestamp)
        {
            assert(curr->type >= COLUMN_TYPE_DATE && curr->type <= COLUMN_TYPE_DATETIME);
            strcat(type, "int64_t");
        }
        else
        {
            if (curr->is_unsigned)
            {
                assert(curr->type >= COLUMN_TYPE_TINY_INT && curr->type <= COLUMN_TYPE_BIG_INT);
                strcat(type, "u");
            }

            strcat(type, types_str[curr->type]);
        }

        if ((!curr->is_unix_timestamp) && \
                (curr->type >= COLUMN_TYPE_DATE && curr->type <= COLUMN_TYPE_DATETIME))
        {
            curr->length = 20;
        }

        unsigned length = curr->length;
        char fmt[20] = { 0 };

        if ((!curr->is_const_length) && \
                (curr->type >= COLUMN_TYPE_CHAR && curr->type <= COLUMN_TYPE_TEXT))
        {
            length += 1;
        }

        if (curr->is_const_length)
        {
            assert(curr->type >= COLUMN_TYPE_CHAR && curr->type <= COLUMN_TYPE_BLOB);
            snprintf(fmt, sizeof(fmt), "%uc", length);
        }
        else if (curr->is_zero_end)
        {
            assert(curr->type >= COLUMN_TYPE_CHAR && curr->type <= COLUMN_TYPE_TEXT);
            snprintf(fmt, sizeof(fmt), "%uS", length);
        }
        else if (curr->is_unix_timestamp)
        {
            assert(curr->type >= COLUMN_TYPE_DATE && curr->type <= COLUMN_TYPE_DATETIME);
            snprintf(fmt, sizeof(fmt), "%c", 'D');
        }
        else
        {
            if (curr->type == COLUMN_TYPE_CHAR || curr->type == COLUMN_TYPE_TINY_TEXT)
            {
                len_type = "uint8_t";
                snprintf(fmt, sizeof(fmt), "-%us", length);
            }
            else if (curr->type == COLUMN_TYPE_VARCHAR || curr->type == COLUMN_TYPE_TEXT)
            {
                len_type = "uint16_t";
                snprintf(fmt, sizeof(fmt), "=%us", length);
            }
            else if (curr->type == COLUMN_TYPE_BINARY || curr->type == COLUMN_TYPE_TINY_BLOB)
            {
                len_type = "uint8_t";
                snprintf(fmt, sizeof(fmt), "-%uc", length);
            }
            else if (curr->type == COLUMN_TYPE_VARBINARY || curr->type == COLUMN_TYPE_BLOB)
            {
                len_type = "uint16_t";
                snprintf(fmt, sizeof(fmt), "=%uc", length);
            }
            else if (curr->type >= COLUMN_TYPE_DATE && curr->type <= COLUMN_TYPE_DATETIME)
            {
                len_type = "uint8_t";
                snprintf(fmt, sizeof(fmt), "-%us", length);
            }
            else
            {
                snprintf(fmt, sizeof(fmt), "%s", packf_str[curr->type]);
            }

            if (curr->type >= COLUMN_TYPE_CHAR)
                ph("    %-20s %s_len;", len_type, curr->name);
        }

        if (curr->is_unix_timestamp)
        {
            ph("    %-20s %s;", type, curr->name);
        }
        else if (curr->type >= COLUMN_TYPE_CHAR && curr->type <= COLUMN_TYPE_DATETIME)
        {
            ph("    %-20s %s[%u];", type, curr->name, length);
        }
        else
        {
            ph("    %-20s %s;", type, curr->name);
        }

        strcat(packf_fmt, fmt);

        if ((line_num == 0 && strlen(packf_fmt) >= 30) || \
                (line_num && strlen(packf_fmt) >= (30 + 72 * line_num + 1)))
        {
            strcat(packf_fmt, "\"\n                \"");
            ++line_num;
        }

        curr = curr->next;
    }

    strcat(packf_fmt, "]");

    ph("} log_%s;", settings.server_name);
    ph();
    ph("# pragma pack()");
    ph();
    ph("int send_%s_log(struct sockaddr_in const *addr, log_%s const *log);", \
            settings.server_name, settings.server_name);
    ph();
    ph("/* Exec a sql statement");
    ph(" * Don't support SELECT or other which return result */");
    ph("int send_%s_sql(struct sockaddr_in const *addr, char const *fmt, ...)", \
            settings.server_name);
    ph("    __attribute__ ((format(printf, 2, 3)));");
    ph();

    return 0;
}

static int generate_c_api_c(void)
{
    pc("# include <stdio.h>");
    pc("# include <string.h>");
    pc("# include <netinet/in.h>");
    pc("# include <sys/socket.h>");
    pc("# include <arpa/inet.h>");
    pc();
    pc("# include \"packf.h\"");
    pc("# include \"%s\"", basepath(settings.api_head_path));
    pc();
    pc("enum");
    pc("{");
    pc("    LOGDB_COMMAND_LOG,");
    pc("    LOGDB_COMMAND_SQL,");
    pc("};");
    pc();
    pc("# pragma pack(1)");
    pc("typedef struct");
    pc("{");
    pc("    %-20s result;", "uint8_t");
    pc("    %-20s command;", "uint8_t");
    pc("    %-20s sequence;", "uint32_t");
    pc("    %-20s echo_len;", "uint16_t");
    pc("    %-20s echo_buf[100];", "uint8_t");
    pc("} log_head;");
    pc("# pragma pack()");
    pc();
    pc("static int sockfd = 0;");
    pc("static int init_flag = 0;");
    pc();
    pc("int send_%s_log(struct sockaddr_in const *addr, log_%s const *log)", \
            settings.server_name, settings.server_name);
    pc("{");
    pc("    if (init_flag == 0)");
    pc("    {");
    pc("        sockfd = socket(AF_INET, SOCK_DGRAM, 0);");
    pc("        if (sockfd < 0)");
    pc("            return -__LINE__;");
    pc();
    pc("        init_flag = 1;");
    pc("    }");
    pc();
    pc("    log_head head;");
    pc("    memset(&head, 0, sizeof(head));");
    pc("    head.command = LOGDB_COMMAND_LOG;");
    pc();
    pc("    char buf[UINT16_MAX - 6];");
    pc("    void *p  = buf;");
    pc("    int left = sizeof(buf);");
    pc("    int  len = 0;");
    pc();
    pc("    NEG_RET_LN(len += vpackf(&p, &left, \"[ccd=100c]\", &head));");
    pc("    NEG_RET_LN(len += vpackf(&p, &left, \"%s\", log));", packf_fmt);
    pc("    NEG_RET_LN(sendto(sockfd, buf, len, 0, (struct sockaddr *)addr, sizeof(*addr)));");
    pc();
    pc("    return 0;");
    pc("}");
    pc();
    pc("int send_%s_sql(struct sockaddr_in const *addr, char const *fmt, ...)", settings.server_name);
    pc("{");
    pc("    if (init_flag == 0)");
    pc("    {");
    pc("        sockfd = socket(AF_INET, SOCK_DGRAM, 0);");
    pc("        if (sockfd < 0)");
    pc("            return -__LINE__;");
    pc();
    pc("        init_flag = 1;");
    pc("    }");
    pc();
    pc("    log_head head;");
    pc("    memset(&head, 0, sizeof(head));");
    pc("    head.command = LOGDB_COMMAND_SQL;");
    pc();
    pc("    char buf[UINT16_MAX - 6];");
    pc("    void *p  = buf;");
    pc("    int left = sizeof(buf);");
    pc("    int  len = 0;");
    pc();
    pc("    NEG_RET_LN(len += vpackf(&p, &left, \"[ccd=100c]\", &head));");
    pc();
    pc("    va_list args;");
    pc("    va_start(args, fmt);");
    pc("    NEG_RET_LN(len += vsnprintf((char *)p, left, fmt, args));");
    pc("    va_end(args);");
    pc("    len += 1;");
    pc();
    pc("    NEG_RET_LN(sendto(sockfd, buf, len, 0, (struct sockaddr *)addr, sizeof(*addr)));");
    pc();
    pc("    return 0;");
    pc("}");
    pc();

    return 0;
}

static int generate_c_api(void)
{
    fph = fopen(settings.api_head_path, "w+");
    if (fph == NULL)
    {
        fprintf(stderr, "open file: %s fail\n", settings.api_head_path);
        return -__LINE__;
    }

    fpc = fopen(settings.api_source_path, "w+");
    if (fpc == NULL)
    {
        fprintf(stderr, "open file: %s fail\n", settings.api_source_path);
        return -__LINE__;
    }

    if (generate_h_api_h() < 0)
        return -__LINE__;
    if (generate_c_api_c() < 0)
        return -__LINE__;

    fclose(fph);
    fclose(fpc);

    return 0;
}

int generate_api(void)
{
    return generate_c_api();
}

