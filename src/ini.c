/*
 * Description: simple read only ini parser
 *     History: damonyang@tencent.com, 2013/06/13, create
 */


# undef  _GNU_SOURCE
# define _GNU_SOURCE

# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <stdbool.h>
# include <ctype.h>
# include <arpa/inet.h>

# include "ini.h"

static bool is_comment(char **line)
{
    char *content = *line;
    while (isspace(*content))
        ++content;

    if (*content == ';' || *content == '#' || *content == '\0')
        return true;

    char *end = content + strlen(content) - 1;
    while (isspace(*end))
        *end-- = '\0';

    *line = content;

    return false;
}

static ssize_t _getline(char **lineptr, size_t *n, FILE *stream)
{
    ssize_t len = getline(lineptr, n, stream);
    if (len == -1)
        return -1;

    char  *_line = NULL;
    size_t _n    = 0;

    while (len >= 2 && (*lineptr)[len - 2] == '\\')
    {
        if (getline(&_line, &_n, stream) == -1)
        {
            free(_line);

            return 0;
        }

        char *next_line = _line;
        while (isspace(*next_line))
            ++next_line;
        ssize_t next_len = strlen(next_line);
        ssize_t need_len = len - 1 + next_len + 1;

        if (*n < (size_t)need_len)
        {
            while (*n < (size_t)need_len)
                *n *= 2;

            *lineptr = realloc(*lineptr, *n);
            if (*lineptr == NULL)
            {
                free(_line);

                return -1;
            }
        }

        if (isspace((*lineptr)[len - 3]))
            (*lineptr)[len - 2] = '\0';
        else
            (*lineptr)[len - 2] = ' ';
        (*lineptr)[len - 1] = '\0';

        strcat(*lineptr, next_line);
        len = strlen(*lineptr);
    }

    if (_line)
        free(_line);

    return len;
}

void ini_free(ini_t *handler)
{
    struct ini_section *curr = handler;
    struct ini_section *next = NULL;

    while (curr)
    {
        next = curr->next;

        struct ini_arg *arg_curr = curr->args;
        struct ini_arg *arg_next = NULL;

        while (arg_curr)
        {
            arg_next = arg_curr->next;

            free(arg_curr->name);
            free(arg_curr->value);
            free(arg_curr);

            arg_curr = arg_next;
        }

        free(curr->name);
        free(curr);

        curr = next;
    }

    return;
}

static void ini_print(ini_t *handler)
{
# ifdef DEBUG
    struct ini_section *curr = handler;

    while (curr)
    {
        if (curr->name == NULL)
            continue;

        printf("[%s]\n", curr->name);

        struct ini_arg *arg = curr->args;

        while (arg)
        {
            if (arg->name == NULL || arg->value == NULL)
                continue;

            printf("    %-20s = %s\n", arg->name, arg->value);
            arg = arg->next;
        }

        curr = curr->next;
    }
# endif

    return;
}

static struct ini_section *create_section(struct ini_section *head, const char *name)
{
    struct ini_section *p = calloc(1, sizeof(struct ini_section));

    if (p == NULL)
    {
        ini_free(head);

        return NULL;
    }

    if ((p->name = strdup(name)) == NULL)
    {
        ini_free(head);

        return NULL;
    }

    return p;
}

static struct ini_section *find_section(struct ini_section *head, const char *name)
{
    struct ini_section *curr = head;

    while (curr)
    {
        if (curr->name && strcmp(curr->name, name) == 0)
            return curr;

        curr = curr->next;
    }

    return NULL;
}

static struct ini_arg *create_arg(struct ini_section *head, const char *name, const char *value)
{
    struct ini_arg *p = calloc(1, sizeof(struct ini_arg));

    if (p == NULL)
    {
        ini_free(head);

        return NULL;
    }

    if ((p->name = strdup(name)) == NULL)
    {
        ini_free(head);

        return NULL;
    }

    if ((p->value = strdup(value)) == NULL)
    {
        ini_free(head);

        return NULL;
    }

    return p;
}

static struct ini_arg *find_arg(struct ini_section *curr, const char *name)
{
    struct ini_arg *arg = curr->args;

    while (arg)
    {
        if (arg->name && strcmp(arg->name, name) == 0)
            return arg;

        arg = arg->next;
    }

    return NULL;
}

ini_t *ini_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return NULL;

    struct ini_section *head = NULL;
    struct ini_section *prev = NULL;
    struct ini_section *curr = NULL;

    struct ini_arg *arg_curr = NULL;
    struct ini_arg *arg_prev = NULL;

    char *line  = NULL;
    size_t   n  = 0;
    ssize_t len = 0;

    while ((len = _getline(&line, &n, fp)) != -1)
    {
        char *s = line;
        if (is_comment(&s))
            continue;
        len = strlen(s);

        if (len >= 3 && s[0] == '[' && s[len - 1] == ']')
        {
            char *name = s + 1;
            while (isspace(*name))
                ++name;

            char *name_end = s + len - 1;
            *name_end-- = '\0';
            while (isspace(*name_end))
                *name_end-- = '\0';

            if ((curr = find_section(head, name)) == NULL)
            {
                if ((curr = create_section(head, name)) == NULL)
                {
                    free(line);

                    return NULL;
                }

                if (head == NULL)
                    head = curr;
                if (prev != NULL)
                    prev->next = curr;

                prev = curr;
                arg_prev = NULL;
            }
            else
            {
                arg_prev = curr->args;
                while (arg_prev->next != NULL)
                    arg_prev = arg_prev->next;
            }

            continue;
        }

        char *delimiter = strchr(s, '=');
        if (delimiter == NULL)
            continue;
        *delimiter = '\0';

        char *name = s;
        char *name_end = delimiter - 1;
        while (isspace(*name_end))
            *name_end-- = '\0';

        char *value = delimiter + 1;
        while (isspace(*value))
            value++;

        if (curr == NULL)
        {
            if ((curr = create_section(head, "global")) == NULL)
            {
                free(line);

                return NULL;
            }

            if (head == NULL)
                head = curr;
            prev = curr;
            arg_prev = NULL;
        }

        if ((arg_curr = find_arg(curr, name)) == NULL)
        {
            arg_curr = create_arg(head, name, value);
            if (arg_curr == NULL)
            {
                free(line);

                return NULL;
            }

            if (arg_prev)
                arg_prev->next = arg_curr;
            if (curr->args == NULL)
                curr->args = arg_curr;

            arg_prev = arg_curr;
        }
        else
        {
            char *old_value = arg_curr->value;

            if ((arg_curr->value = strdup(value)) == NULL)
            {
                ini_free(head);

                free(line);

                return NULL;
            }

            free(old_value);
        }
    }

    free(line);
    fclose(fp);

    if (head == NULL)
    {
        if ((head = calloc(1, sizeof(struct ini_section))) == NULL)
            return NULL;
    }

    ini_print(head);

    return head;
}

int ini_read_str(ini_t *handler, const char *section,
        const char *name, char **value, const char *default_value)
{
    if (!handler || !name || !value)
        return -1;

    if (section == NULL || *section == 0)
        section = "global";

    struct ini_section *curr = handler;

    while (curr)
    {
        if (curr->name && strcmp(section, curr->name) == 0)
            break;

        curr = curr->next;
    }

    if (curr)
    {
        struct ini_arg *arg = curr->args;

        while (arg)
        {
            if (arg->name && arg->value && strcmp(arg->name, name) == 0)
            {
                *value = strdup(arg->value);
                if (*value == NULL)
                    return -1;

                return 0;
            }

            arg = arg->next;
        }
    }

    if (default_value)
    {
        *value = strdup(default_value);
        if (*value == NULL)
            return -1;
    }
    else
    {
        *value = NULL;
    }

    return 1;
}

static char *sstrncpy(char *dest, const char *src, size_t n)
{
    if (n == 0)
        return dest;

    dest[0] = 0;

    return strncat(dest, src, n - 1);
}

int ini_read_strn(ini_t *handler, const char *section,
        const char *name, char *value, size_t n, const char *default_value)
{
    char *s = NULL;
    int ret = ini_read_str(handler, section, name, &s, default_value);
    if (ret < 0)
        return ret;

    memset(value, 0, n);

    if (s)
    {
        sstrncpy(value, s, n);
        free(s);
    }

    return ret;
}

static int ini_read_num(ini_t *handler, const char *section,
        const char *name, void *value, bool is_unsigned)
{
    char *s = NULL;
    int ret = ini_read_str(handler, section, name, &s, NULL);
    if (ret == 0)
    {
        if (is_unsigned)
            *(unsigned long long int*)value = strtoull(s, NULL, 0);
        else
            *(long long int *)value = strtoll(s, NULL, 0);

        free(s);
    }

    return ret;
}

# define INI_READ_SIGNED(type) do { \
    long long int v; \
    int ret = ini_read_num(handler, section, name, &v, false); \
    if (ret == 0) { \
        *value = (type)v; \
    } \
    else if (ret > 0) { \
        *value = default_value; \
    } \
    return ret; \
} while (0)

# define INI_READ_UNSIGNED(type) do { \
    unsigned long long int v; \
    int ret = ini_read_num(handler, section, name, &v, true); \
    if (ret == 0) { \
        *value = (type)v; \
    } \
    else if (ret > 0) { \
        *value = default_value; \
    } \
    return ret; \
} while (0)

int ini_read_int(ini_t *handler, const char *section,
        const char *name, int *value, int default_value)
{
    INI_READ_SIGNED(int);
}

int ini_read_unsigned(ini_t *handler, const char *section,
        const char *name, unsigned *value, unsigned default_value)
{
    INI_READ_UNSIGNED(unsigned);
}

int ini_read_int8(ini_t *handler, const char *section,
        const char *name, int8_t *value, int8_t default_value)
{
    INI_READ_SIGNED(int8_t);
}

int ini_read_uint8(ini_t *handler, const char *section,
        const char *name, uint8_t *value, uint8_t default_value)
{
    INI_READ_UNSIGNED(uint8_t);
}

int ini_read_int16(ini_t *handler, const char *section,
        const char *name, int16_t *value, int16_t default_value)
{
    INI_READ_SIGNED(int16_t);
}

int ini_read_uint16(ini_t *handler, const char *section,
        const char *name, uint16_t *value, uint16_t default_value)
{
    INI_READ_UNSIGNED(uint16_t);
}

int ini_read_int32(ini_t *handler, const char *section,
        const char *name, int32_t *value, int32_t default_value)
{
    INI_READ_SIGNED(int32_t);
}

int ini_read_uint32(ini_t *handler, const char *section,
        const char *name, uint32_t *value, uint32_t default_value)
{
    INI_READ_UNSIGNED(uint32_t);
}

int ini_read_int64(ini_t *handler, const char *section,
        const char *name, int64_t *value, int64_t default_value)
{
    INI_READ_SIGNED(int64_t);
}

int ini_read_uint64(ini_t *handler, const char *section,
        const char *name, uint64_t *value, uint64_t default_value)
{
    INI_READ_UNSIGNED(uint64_t);
}

int ini_read_float(ini_t *handler, const char *section,
        const char *name, float *value, float default_value)
{
    char *s = NULL;
    int ret = ini_read_str(handler, section, name, &s, NULL);
    if (ret == 0)
    {
        *value = strtof(s, NULL);

        free(s);
    }
    else if (ret > 0)
    {
        *value = default_value;
    }

    return ret;
}

int ini_read_double(ini_t *handler, const char *section,
        const char *name, double *value, double default_value)
{
    char *s = NULL;
    int ret = ini_read_str(handler, section, name, &s, NULL);
    if (ret == 0)
    {
        *value = strtod(s, NULL);

        free(s);
    }
    else if (ret > 0)
    {
        *value = default_value;
    }

    return ret;
}

int ini_read_ipv4_addr(ini_t *handler, const char *section,
        const char *name, struct sockaddr_in *addr, const char *default_value)
{
    char *s = NULL;
    int ret = ini_read_str(handler, section, name, &s, default_value);
    if (ret < 0)
        return ret;

    memset(addr, 0, sizeof(struct sockaddr_in));

    if (s)
    {
        char *ip = strtok(s, ": \t");
        if (ip == NULL)
        {
            free(s);

            return -1;
        }

        char *port = strtok(NULL, ": \t");
        if (port == NULL)
        {
            free(s);

            return -1;
        }

        addr->sin_family = AF_INET;
        if (inet_aton(ip, &addr->sin_addr) == 0)
        {
            free(s);

            return -1;
        }

        addr->sin_port = htons((uint16_t)atoi(port));

        free(s);
    }

    return ret;
}

int ini_read_bool(ini_t *handler, const char *section,
        const char *name, bool *value, bool default_value)
{
    char *s = NULL;
    int ret = ini_read_str(handler, section, name, &s, NULL);
    if (ret == 0)
    {
        int i;
        for (i = 0; s[i]; ++i)
            s[i] = tolower(s[i]);

        if (strcmp(s, "true") == 0)
            *value = true;
        else if (strcmp(s, "false") == 0)
            *value = false;
        else
            *value = default_value;

        free(s);
    }
    else if (ret > 0)
    {
        *value = default_value;
    }

    return ret;
}

