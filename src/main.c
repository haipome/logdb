/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/17, create
 */

# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <stdbool.h>
# include <ctype.h>
# include <limits.h>
# include <unistd.h>
# include <getopt.h>
# include <error.h>
# include <errno.h>
# include <signal.h>
# include <inttypes.h>

# include "queue.h"
# include "conf.h"
# include "utils.h"
# include "db.h"
# include "net.h"
# include "job.h"
# include "sql.h"
# include "seq.h"
# include "api.h"

int shut_down_flag;
static char config_file_path[PATH_MAX];

# define PACKAGE_STRING "logdb"
# define VERSION_STRING "1.4"

static void show_usage(void)
{
    printf( "Usage:\n"
            "  -h, --help       show logdb version, usage, and exit\n"
            "\n"
            "  -c, --config=S   assign the config file path\n"
            "\n"
            "  -f  --offset=N   offset time\n"
            "  -s  --syncdb     sync database\n"
            "  -m  --merge      create merge table\n"
            "\n"
            "  -a  --api        generate api\n"
            "\n"
            "  -q  --queue-stat print queue status\n"
            "  -r  --rm-queue   rm all queue shm by call ipcrm\n"
            "\n"
            "Report bugs to <damonyang@tencent.com>\n");
}

static int offset_time = 0;

static int sync_database_flag = false;
static int generate_api_flag  = false;
static int queue_status_flag  = false;
static int rm_queue_shm_flag  = false;
static int create_merge_flag  = false;

static void get_options(int argc, char *argv[])
{
    if (argc == 1)
    {
        show_usage();

        exit(EXIT_FAILURE);
    }

    struct option long_options[] =
    {
        { "help",               no_argument,        NULL,   'h' },
        { "config",             required_argument,  NULL,   'c' },
        { "offset",             required_argument,  NULL,   'f' },
        { "syncdb",             no_argument,        NULL,   's' },
        { "merge",              no_argument,        NULL,   'm' },
        { "api",                no_argument,        NULL,   'a' },
        { "queue-stat",         no_argument,        NULL,   'q' },
        { "rm-queue",           no_argument,        NULL,   'r' },
        { NULL,                 0,                  NULL,    0  },
    };

    char short_options[] = 
        "h"
        "c:"
        "f:"
        "s"
        "m"
        "a"
        "q"
        "r"
        ;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'h':
            show_usage();

            exit(EXIT_SUCCESS);
        case 'c':
            sstrncpy(config_file_path, optarg, sizeof(config_file_path));
            break;
        case 's':
            sync_database_flag = true;
            break;
        case 'f':
            offset_time = atoi(optarg);
            if (offset_time > 0)
                error(EXIT_FAILURE, errno, "offset_time shoule be a negative value");

            break;
        case 'a':
            generate_api_flag = true;
            break;
        case 'm':
            create_merge_flag = true;
            break;
        case 'q':
            queue_status_flag = true;
            break;
        case 'r':
            rm_queue_shm_flag = true;
            break;
        case '?':
            exit(EXIT_FAILURE);
        default:
            abort();
        }
    }
}

static void create_merge(void)
{
    char *merge_sql = create_merge_table_sql(offset_time);
    if (merge_sql == NULL)
        error(EXIT_FAILURE, errno, "generate create merge table sql fail");

    char *merge_table_name = get_table_name(-1, offset_time);
    if (merge_table_name == NULL)
        error(EXIT_FAILURE, errno, "get merge table name fail");

    char drop_sql[1024];
    snprintf(drop_sql, sizeof(drop_sql), "DROP TABLE IF EXISTS `%s`", merge_table_name);

    int ret;

    printf("exec sql: \n%s\n\n", drop_sql);
    ret = db_query(drop_sql, 0);
    if (ret < 0)
        error(EXIT_FAILURE, errno, "drop table %s fail: %s", merge_table_name, db_error());

    printf("exec sql: \n%s\n\n", merge_sql);
    ret = db_query(merge_sql, 0);
    if (ret < 0)
        error(EXIT_FAILURE, errno, "create merge table fail: %s", db_error());

    printf("create merge table: %s success!\n", merge_table_name);

    return;
}

static void add_column(struct column *target)
{
    int i, ret;
    for (i = 0; i < settings.hash_table_num; ++i)
    {
        char *table_name = get_table_name(i, offset_time);
        if (table_name == NULL)
            error(EXIT_FAILURE, errno, "get table name fail");

        char *alter_sql = add_column_sql(table_name, target);
        if (alter_sql == NULL)
            error(EXIT_FAILURE, errno, "generate add column sql fail");

        printf("exec sql:\n%s\n", alter_sql);
        ret = db_query(alter_sql, 0);
        if (ret < 0)
        {
            fprintf(stderr, "table %s add column %s fail: %s\n", \
                    table_name, target->name, db_error());
        }
        else
        {
            printf("table %s add column %s success!\n\n", \
                    table_name, target->name);
        }
    }

    return;
}

static void add_key(bool is_primary, char *name)
{
    int i, ret;
    for (i = 0; i < settings.hash_table_num; ++i)
    {
        char *table_name = get_table_name(i, offset_time);
        if (table_name == NULL)
            error(EXIT_FAILURE, errno, "get table name fail");

        char alter_sql[1024];
        if (is_primary)
        {
            snprintf(alter_sql, sizeof(alter_sql), "ALTER TABLE `%s` ADD PRIMARY KEY (`%s`)", \
                    table_name, name);
        }
        else
        {
            snprintf(alter_sql, sizeof(alter_sql), "ALTER TABLE `%s` ADD INDEX `index_%s` (`%s`)", \
                    table_name, name, name);
        }

        printf("exec sql:\n%s\n", alter_sql);
        ret = db_query(alter_sql, 0);
        if (ret < 0)
        {
            fprintf(stderr, "table %s add key %s fail: %s\n", \
                    table_name, name, db_error());
        }
        else
        {
            printf("table %s add key %s success!\n\n", table_name, name);
        }
    }

    return;
}

static void change_column(struct column *target)
{
    int i, ret;
    for (i = 0; i < settings.hash_table_num; ++i)
    {
        char *table_name = get_table_name(i, offset_time);
        if (table_name == NULL)
            error(EXIT_FAILURE, errno, "get table name fail");

        char *alter_sql = change_column_sql(table_name, target);
        if (alter_sql == NULL)
            error(EXIT_FAILURE, errno, "generate change column sql fail");

        printf("exec sql:\n%s\n", alter_sql);
        ret = db_query(alter_sql, 0);
        if (ret < 0)
        {
            error(EXIT_FAILURE, errno, "table: %s change column: %s fail: %s\n", \
                    table_name, target->name, db_error());
        }
        else
        {
            printf("table: %s change column: %s success!\n\n", \
                    table_name, target->name);
        }
    }
}

static struct column *find_field(int num, struct column *set, char *name)
{
    int i;
    for (i = 0; i < num; ++i)
    {
        if (strcmp(set[i].name , name) == 0)
            return &set[i];
    }

    return NULL;
}

static bool is_new_append(int num, struct column *set, struct column *target)
{
    struct column *curr = target;
    while (curr)
    {
        if (find_field(num, set, curr->name))
            return false;

        curr = curr->next;
    }

    return true;
}

static void sync_db(void)
{
    char *table_name = get_table_name(settings.hash_table_num - 1, offset_time);
    if (table_name == NULL)
        error(EXIT_FAILURE, errno, "get table name fail");

    char *new_table_sql = create_table_sql(table_name);
    if (new_table_sql == NULL)
        error(EXIT_FAILURE, errno, "get create table sql fail");

    printf("table definition:\n%s\n", new_table_sql);

    if (ask_is_continue() == false)
        exit(EXIT_FAILURE);

    int num;
    struct column *fields;
    struct column *field;

    int ret = db_desc_table(table_name, &num, &fields);
    if (ret < 0)
    {
        if (ret == -1)
            error(EXIT_FAILURE, 0, "syncdb: fail, You may need to first start the server");
        else
            error(EXIT_FAILURE, errno, "desc table %s fail: %s", table_name, db_error());
    }

    int alter_num = 0;
    struct column *curr = settings.columns;
    while (curr)
    {
        if (curr->is_storage == false)
        {
            curr = curr->next;
            continue;
        }

        field = find_field(num, fields, curr->name);

        if (field == NULL)
        {
            if (is_new_append(num, fields, curr) == false)
            {
                fprintf(stderr, "ERROR: new column: `%s` NOT at the end, this is NOT compatible!\n", curr->name);
                exit(EXIT_FAILURE);
            }

            printf("need add column: %s\n", curr->name);

            alter_num += 1;
            curr->alter_type = ALTER_ADD;
        }

        if (curr->is_index && (field == NULL || field->is_index == false))
        {
            printf("need add index: %s\n", curr->name);

            alter_num += 1;
            curr->alter_key_type = ALTER_ADD_INDEX;
        }
        else if (curr->is_primary && curr->is_auto_increment == false \
                && (field == NULL || field->is_primary == false))
        {
            printf("need add primary key: %s\n", curr->name);

            alter_num += 1;
            curr->alter_key_type = ALTER_ADD_PRIMARY;
        }

        if (field && ((field->type != curr->type) || ( \
                        (curr->type >= COLUMN_TYPE_TINY_INT) && \
                        (curr->type <= COLUMN_TYPE_BIG_INT) && \
                        (field->is_unsigned != curr->is_unsigned)))
           )
        {
            fprintf(stderr, "WARN: column: `%s` type changed, this may be not compatible!\n", curr->name);
            if (ask_is_continue() == false)
                exit(EXIT_FAILURE);

            printf("need change column: %s\n", curr->name);

            alter_num += 1;
            curr->alter_type = ALTER_CHANGE;
        }

        if (field && field->type == curr->type && field->length != curr->length && \
                (curr->type == COLUMN_TYPE_CHAR || curr->type == COLUMN_TYPE_VARCHAR || \
                 curr->type == COLUMN_TYPE_BINARY || curr->type == COLUMN_TYPE_VARBINARY))
        {
            printf("need change column length: %s, %u -> %u\n", \
                    curr->name, field->length, curr->length);

            alter_num += 1;
            curr->alter_type = ALTER_CHANGE_LEN;
        }

        if (field && field->is_auto_increment != curr->is_auto_increment && \
                curr->type >= COLUMN_TYPE_TINY_INT && curr->type <= COLUMN_TYPE_BIG_INT)
        {
            printf("need change column: %s\n", curr->name);

            alter_num += 1;
            curr->alter_type = ALTER_CHANGE;
        }

        curr = curr->next;
    }

    if (alter_num == 0)
    {
        printf("syncdb: nothing need to do\n");

        exit(EXIT_SUCCESS);
    }

    if (ask_is_continue() == false)
        exit(EXIT_FAILURE);

    curr = settings.columns;
    while (curr)
    {
        if (curr->is_storage == false || \
                (curr->alter_type == 0 && curr->alter_key_type == 0))
        {
            curr = curr->next;
            continue;
        }

        field = find_field(num, fields, curr->name);

        switch (curr->alter_type)
        {
        case ALTER_ADD:
            printf("add column: %s\n", curr->name);
            add_column(curr);

            break;
        case ALTER_CHANGE:
            printf("change column: %s\n", curr->name);
            change_column(curr);

            break;

        case ALTER_CHANGE_LEN:
            printf("change column length: %s\n", curr->name);
            change_column(curr);

            break;
        }

        switch (curr->alter_key_type)
        {
        case ALTER_ADD_INDEX:
            printf("add index: %s\n", curr->name);
            add_key(false, curr->name);

            break;
        case ALTER_ADD_PRIMARY:
            printf("add primary key: %s\n", curr->name);
            add_key(true, curr->name);

            break;
        }

        curr = curr->next;
    }

    if (settings.hash_table_num > 1 && strcasecmp(settings.db_engine, "myisam") == 0)
    {
        create_merge();
    }

    return;
}

static int init_worker_queue(int i)
{
    char bin_file[PATH_MAX];
    snprintf(bin_file, sizeof(bin_file), "%s_%d", settings.queue_bin_file_path, i);

    int ret = queue_init(&settings.workers[i].queue, settings.server_name, \
            settings.queue_base_shm_key + i - 1, \
            settings.queue_mem_cache_size, bin_file, settings.queue_bin_file_max_size);
    if (ret < 0)
    {
        fprintf(stderr, "init worker %d queue fail: %d, shm key may has been used!\n", i, ret);

        return -__LINE__;
    }

    return 0;
}

static int init_cache_queue(int i)
{
    char bin_file[PATH_MAX];
    snprintf(bin_file, sizeof(bin_file), "%s_cache_%d", settings.queue_bin_file_path, i);

    int ret = queue_init(&settings.cache_queue, NULL, 0, \
            settings.queue_mem_cache_size, bin_file, settings.queue_bin_file_max_size);
    if (ret < 0)
    {
        fprintf(stderr, "init worker %d cache queue fail: %d\n", i, ret);

        return -__LINE__;
    }

    return 0;
}

static void print_queue_stat(void)
{
    settings.workers = calloc(settings.worker_proc_num + 1, sizeof(struct worker));
    if (settings.workers == NULL)
        error(EXIT_FAILURE, errno, "calloc fail");

    int i;
    for (i = 1; i <= settings.worker_proc_num; ++i)
    {
        if (init_worker_queue(i) < 0)
            exit(EXIT_FAILURE);
    }

    printf("%-4s %-10s %-10s %-10s %s\n", \
            "id", "mem unit", "mem size", "file unit", "file size");

    for (i = 1; i <= settings.worker_proc_num; ++i)
    {
        uint32_t mem_unit = 0;
        uint32_t mem_size = 0;
        uint32_t file_unit = 0;
        uint64_t file_size = 0;

        queue_stat(&settings.workers[i].queue, &mem_unit, &mem_size, &file_unit, &file_size);

        printf("%-4d %-10u %-10u %-10u %"PRIu64"\n", \
                i, mem_unit, mem_size, file_unit, file_size);
    }

    return;
}

static void rm_all_queue_shm(void)
{
    int i;
    for (i = 1; i <= settings.worker_proc_num; ++i)
    {
        char cmd[100];
        snprintf(cmd, sizeof(cmd), "ipcrm -M %d", settings.queue_base_shm_key + i - 1);

        puts(cmd);
        system(cmd);
    }

    return;
}

static void handle_options(void)
{
    if (sync_database_flag)
    {
        sync_db();

        exit(EXIT_SUCCESS);
    }

    if (create_merge_flag)
    {
        if (settings.hash_table_num <= 1)
            error(EXIT_FAILURE, 0, "don't need create merge table");

        if (strcasecmp(settings.db_engine, "myisam") != 0)
            error(EXIT_FAILURE, 0, "not MyISAM engine");

        create_merge();

        exit(EXIT_SUCCESS);
    }

    if (generate_api_flag)
    {
        int ret = generate_api();
        if (ret < 0)
            error(EXIT_FAILURE, errno, "generate api fail: %d", ret);

        printf("generate api success!\n");

        exit(EXIT_SUCCESS);
    }

    if (queue_status_flag)
    {
        print_queue_stat();

        exit(EXIT_SUCCESS);
    }

    if (rm_queue_shm_flag)
    {
        rm_all_queue_shm();

        exit(EXIT_SUCCESS);
    }
}

static int create_worker_proc(void)
{
    settings.workers = calloc(settings.worker_proc_num + 1, sizeof(struct worker));
    if (settings.workers == NULL)
        return -__LINE__;

    int i;
    for (i = 1; i <= settings.worker_proc_num; ++i)
    {
        if (pipe(settings.workers[i].pipefd) != 0)
            return -__LINE__;

        pid_t pid = fork();
        if (pid < 0)
            return -__LINE__;

        if (pid == 0)
        {
            settings.worker_id = i;

            int j;
            for (j = 1; j <= i; ++j)
            {
                close(settings.workers[j].pipefd[1]);
                settings.workers[j].pipefd[1] = 0;
            }

            break;
        }

        close(settings.workers[i].pipefd[0]);
        settings.workers[i].pipefd[0] = 0;
        settings.workers[i].pid = pid;
    }

    if (settings.worker_id == 0)
    {
        settings.workers[0].pid = getpid();

        for (i = 1; i <= settings.worker_proc_num; ++i)
        {
            NEG_RET_LN(init_worker_queue(i));
        }
    }
    else
    {
        NEG_RET_LN(init_worker_queue(settings.worker_id));
        NEG_RET_LN(init_cache_queue(settings.worker_id));
    }

    return 0;
}

static int init_logs(void)
{
    default_dlog = dlog_init(settings.default_log_path, \
            DLOG_SHIFT_BY_DAY | DLOG_USE_FORK, 1024 * 1024 * 1024, 10, 30);
    if (default_dlog == NULL)
        return -__LINE__;

    default_dlog_flag = dlog_read_flag(settings.default_log_flag);

    settings.fail_enqueue_log = dlog_init(settings.fail_enqueue_log_path, \
            DLOG_SHIFT_BY_DAY | DLOG_USE_FORK, 1024 * 1024 * 1024, 0, 0);
    if (settings.fail_enqueue_log == NULL)
        return -__LINE__;

    settings.fail_insert_log = dlog_init(settings.fail_insert_log_path, \
            DLOG_SHIFT_BY_DAY, 1024 * 1024 * 1024, 0, 0);
    if (settings.fail_insert_log == NULL)
        return -__LINE__;

    return 0;
}

static void kill_all_worker(void)
{
    int i;
    for (i = 0; i <= settings.worker_proc_num; ++i)
    {
        if (settings.workers[i].pid)
            kill(settings.workers[i].pid, SIGQUIT);
    }
}

static void handle_signal(int signo)
{
    switch (signo)
    {
    case SIGQUIT:
        shut_down_flag = true;

        break;
    }
}

int main(int argc, char *argv[])
{
    printf("%s-%s, compile in: %s %s, now: %s\n", \
            PACKAGE_STRING, VERSION_STRING, __DATE__, __TIME__, get_curr_date_time());

    get_options(argc, argv);

    if (config_file_path[0] == 0)
    {
        fprintf(stderr, "please use '-c' to assign config file\n");

        exit(EXIT_FAILURE);
    }

    int ret;

    ret = read_settings(config_file_path);
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "read settings from config file %s fail: %d",
                config_file_path, ret);
    }

    char path[PATH_MAX] = { 0 };
    chdir(parentpath(realpath(argv[0], path)));

    handle_options();

    if (settings.has_global_sequence)
    {
        ret = sequence_init();
        if (ret < 0)
            error(EXIT_FAILURE, errno, "init global sequence fail: %d", ret);
    }

    char server_name[PATH_MAX];
    snprintf(server_name, sizeof(server_name), "logdb_%s", settings.server_name);

    ret = is_server_exist(server_name);
    if (ret != 0)
    {
        error(EXIT_FAILURE, 0, "server %s may has been exists", settings.server_name);
    }

    /* test db */
    ret = db_connect();
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "connect db fail: %s", db_error());
    }
    db_close();

    /* test net */
    ret = create_udp_socket(settings.local_ip, settings.listen_port);
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "create udp socket fail: %d", ret);
    }
    close_udp_socket();

    ret = init_logs();
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "init logs fail: %d", ret);
    }

    /* close all fd but 0, 1, 2, no change current working directory */
    if (daemon(true, true) < 0)
    {
        error(EXIT_FAILURE, errno, "daemon fail");
    }

    ret = create_worker_proc();
    if (ret < 0 && settings.worker_id == 0)
    {
        kill_all_worker();
        error(EXIT_FAILURE, errno, "create worker process fail: %d", ret);
    }

    signal(SIGQUIT, handle_signal);
    signal(SIGCHLD, SIG_IGN);

    ret = db_connect();
    if (ret < 0)
    {
        error(EXIT_FAILURE, 0, "connect db fail: %s", db_error());
    }

    if (settings.worker_id == 0)
    {
        ret = create_udp_socket(settings.local_ip, settings.listen_port);
        if (ret < 0)
        {
            error(EXIT_FAILURE, errno, "create udp socket fail: %d", ret);
        }
    }

    if (settings.worker_id == 0)
    {
        printf("%s start[%d]\n", basepath(argv[0]), getpid());
        log_vip("reciver start[%d]", getpid());

        do_reciver_job();
    }
    else
    {
        log_vip("worker id: %d, start[%d]", settings.worker_id, getpid());

        do_worker_job();
    }

    return 0;
}

