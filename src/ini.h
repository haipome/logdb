/*
 * Description: simple read only ini parser
 *     History: damonyang@tencent.com, 2013/06/13, create
 */

# pragma once

# include <stdint.h>
# include <stdbool.h>
# include <netinet/in.h>

struct ini_arg
{
    char  *name;
    char  *value;
    struct ini_arg *next;
};

struct ini_section
{
    char  *name;
    struct ini_arg *args;
    struct ini_section *next;
};

typedef struct ini_section ini_t;

/*
 * Feature:
 * 1: If a property name declared befor any section is declared, it
 *    is in a "global" section. If the ini_read_* function argument
 *    section is empty string or NULL, they will find the property
 *    name in the "global" section.
 *
 * 2: There is no technical LIMIT on the length of section name or
 *    property name or value, or the num of sections and properties,
 *    the limit is the size of memory.
 *
 * 3: There is no special limit on the name of section and property.
 *    Note that the surround whitespace of the name of section and
 *    property and value is ignored, but they can contian whitespace.
 *    Note that the name of section and property are case insensitivity.
 *
 * 4: Blank line is ignored.
 *
 * 5: Lines beginning with '#' or ';' are ignored and may be used to
 *    provide comments.
 *
 * 6: The second occurrence of a property name in the same section
 *    overwrite the previous one. The section occurrence of a section
 *    is joined whih the previous one.
 *
 * 7: If a line end with '\', where a backslash followed immediately
 *    by EOL (end-of-line) causes the line break to be ignored, and
 *    the "logical line" to be continued on the next actual line from
 *    the INI file. Example:
 *        name = simple read only ini parser
 *    is the same with:
 *        name = simple \
 *               read only \
 *               ini parser
 */

/*
 * Load a ini config file to memory, return NULL if fail.
 */
ini_t *ini_load(const char *path);

/*
 * Return value:
 * If the combination of section and name found in config file, return 0.
 * Return 1 if not found, the *value will be the default_value.
 *
 * Fail return -1;
 */

/*
 * Read string from ini config handler.
 * The string is allocated using malloc, so, you need free it after use.
 */ 
int ini_read_str(ini_t *handler, const char *section,
        const char *name, char **value, const char *default_value);

/*
 * Read string from ini config handler.
 * If the real length of value is greater than or equal to n, n - 1
 * characters and a null terminator will be copied to value, else
 * the remainder of value pads with null bytes.
 */
int ini_read_strn(ini_t *handler, const char *section,
        const char *name, char *value, size_t n, const char *default_value);

/*
 * Read int or unsigned int or stdint from ini config handler.
 * Support octal or hexadecimal base ("0" or "0x"/"0X" respectively).
 */
int ini_read_int(ini_t *handler, const char *section,
        const char *name, int *value, int default_value);

int ini_read_unsigned(ini_t *handler, const char *section,
        const char *name, unsigned *value, unsigned default_value);

int ini_read_int8(ini_t *handler, const char *section,
        const char *name, int8_t *value, int8_t default_value);

int ini_read_uint8(ini_t *handler, const char *section,
        const char *name, uint8_t *value, uint8_t default_value);

int ini_read_int16(ini_t *handler, const char *section,
        const char *name, int16_t *value, int16_t default_value);

int ini_read_uint16(ini_t *handler, const char *section,
        const char *name, uint16_t *value, uint16_t default_value);

int ini_read_int32(ini_t *handler, const char *section,
        const char *name, int32_t *value, int32_t default_value);

int ini_read_uint32(ini_t *handler, const char *section,
        const char *name, uint32_t *value, uint32_t default_value);

int ini_read_int64(ini_t *handler, const char *section,
        const char *name, int64_t *value, int64_t default_value);

int ini_read_uint64(ini_t *handler, const char *section,
        const char *name, uint64_t *value, uint64_t default_value);

/*
 * Read float/double from ini config handler.
 */
int ini_read_float(ini_t *handler, const char *section,
        const char *name, float *value, float default_value);

int ini_read_double(ini_t *handler, const char *section,
        const char *name, double *value, double default_value);

/*
 * Read a ipv4 addr such as: 127.0.0.1:8080 or 127.0.0.1 8080
 */
int ini_read_ipv4_addr(ini_t *handler, const char *section,
        const char *name, struct sockaddr_in *addr, const char *default_value);

/*
 * Read a bool from ini config handler. The value in the ini config file can be
 * "true" or "false", the case are ignored.
 */
int ini_read_bool(ini_t *handler, const char *section,
        const char *name, bool *value, bool default_value);

/* Free a ini config handler */
void ini_free(ini_t *handler);

