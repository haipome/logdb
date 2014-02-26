/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/07/02, create
 */

# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdbool.h>
# include <string.h>
# include <strings.h>
# include <limits.h>
# include <unistd.h>
# include <getopt.h>
# include <error.h>
# include <errno.h>
# include <signal.h>
# include <inttypes.h>
# include <arpa/inet.h>

# include "utils.h"
# include "ini.h"
# include "timer.h"
# include "net.h"
# include "queue.h"
# include "dlog.h"
# include "protocol.h"
# include "serialize.h"

struct settings
{
    char                *server_name;

    char                *local_ip;
    uint16_t            listen_port;

    char                *default_log_path;
    char                *default_log_flag;

    char                *queue_bin_file_path;
    uint32_t            queue_mem_cache_size;
    uint64_t            queue_bin_file_max_size;
    queue_t             no_reply_cache_queue;

    bool                is_return_pkg;

    struct sockaddr_in  reciver_addr;
};

struct settings settings;

int  shut_down_flag;
char config_file_path[PATH_MAX];
int  recive_reply_flag;

# define PACKAGE_STRING "loginf"
# define VERSION_STRING "1.3"

static void show_usage(void)
{
    printf( "Usage:\n"
            "  -h --help        show loginf version, usage, and exit\n"
            "  -c --config      assign the config file path\n"
            "\n"
            "Report bugs to <damonyang@tencent.com>\n");
}

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
        { NULL,                 0,                  NULL,    0  },
    };

    char short_options[] = 
        "h"
        "c:"
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
        case '?':
            exit(EXIT_FAILURE);
        default:
            abort();
        }
    }
}

static int read_settings(void)
{
    ini_t *conf = ini_load(config_file_path);
    if (conf == NULL)
        return -__LINE__;

    if (ini_read_str(conf, "", "server name", &settings.server_name, NULL) != 0)
    {
        fprintf(stderr, "'server name' is required field\n");

        return -__LINE__;
    }

    if (ini_read_str(conf, "", "local ip", &settings.local_ip, NULL) < 0)
        return -__LINE__;

    if (ini_read_uint16(conf, "", "listen port", &settings.listen_port, 22060) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "default log path", \
                &settings.default_log_path, "../log/default") < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "default log flag", \
                &settings.default_log_flag, "fatal, error, warn, info, notice") < 0)
        return -__LINE__;

    if (ini_read_uint32(conf, "", "queue memory cache size", \
                &settings.queue_mem_cache_size, 8 * 1024 * 1024) < 0)
        return -__LINE__;

    if (ini_read_str(conf, "", "queue bin file path", \
                &settings.queue_bin_file_path, "../binlog/queue") < 0)
        return -__LINE__;

    if (ini_read_uint64(conf, "", "queue bin file max size", \
                &settings.queue_bin_file_max_size, 10 * 1024 * 1024 * 1024ull) < 0)
        return -__LINE__;

    if (ini_read_bool(conf, "", "return pkg", &settings.is_return_pkg, false) < 0)
        return -__LINE__;

    ini_free(conf);

    bzero(&settings.reciver_addr, sizeof(settings.reciver_addr));
    settings.reciver_addr.sin_family = AF_INET;
    settings.reciver_addr.sin_port = htons(settings.listen_port + 1);

    const char *reciver_ip = settings.local_ip;
    if (reciver_ip == NULL)
        reciver_ip = "127.0.0.1";
    if (inet_aton(reciver_ip, &settings.reciver_addr.sin_addr) == 0)
        return -__LINE__;

    return 0;
}

static int init_log(void)
{
    default_dlog = dlog_init(settings.default_log_path, \
            DLOG_SHIFT_BY_DAY | DLOG_USE_FORK, 1024 * 1024 * 1024, 10, 30);
    if (default_dlog == NULL)
        return -__LINE__;

    default_dlog_flag = dlog_read_flag(settings.default_log_flag);

    return 0;
}

static int init_queue(void)
{
    char bin_file[PATH_MAX];
    snprintf(bin_file, sizeof(bin_file), "%s_cache", settings.queue_bin_file_path);

    int ret = queue_init(&settings.no_reply_cache_queue, NULL, 0, \
            settings.queue_mem_cache_size, bin_file, settings.queue_bin_file_max_size);
    if (ret < 0)
        return -__LINE__;

    return 0;
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

static void handle_time_out(uint32_t sequence, size_t size, void *data)
{
    log_warn("time out, seq: %u", sequence);

    int ret = queue_push(&settings.no_reply_cache_queue, data, (uint32_t)size);
    if (ret < 0)
    {
        log_error("queue_push fail: %d\n", ret);
    }

    recive_reply_flag = false;
}

static int send_to_reciver(struct sockaddr_in *client_addr, \
        uint8_t command, uint32_t sequence, void *body, int body_len)
{
    struct protocol_head head;
    bzero(&head, sizeof(head));
    head.command  = command;
    head.sequence = sequence;

    struct inner_addr addr;
    addr.ip = (uint32_t)client_addr->sin_addr.s_addr;
    addr.port = (uint16_t)client_addr->sin_port;
    head.echo_len = sizeof(addr);
    head.echo = &addr;

    char buf[UINT16_MAX];
    void *p = buf;
    int  left = sizeof(buf);

    NEG_RET_LN(add_head(&head, &p, &left));
    NEG_RET_LN(add_bin(&p, &left, body, body_len));

    NEG_RET_LN(send_udp_pkg(buf, sizeof(buf) - left, &settings.reciver_addr));

    return 0;
}

static int return_to_sender(uint16_t result, void *data, size_t data_len)
{
    if (settings.is_return_pkg == false)
        return 0;

    struct sockaddr_in *addr = data;
    void *pkg = data + sizeof(*addr);
    int len = data_len - sizeof(*addr);

    struct protocol_head head;
    void *p = pkg;
    int left = len;
    NEG_RET_LN(get_head(&head, &p, &left));

    head.result = result;

    char buf[UINT16_MAX];
    p = buf;
    left = sizeof(buf);
    NEG_RET_LN(add_head(&head, &p, &left));

    NEG_RET_LN(send_udp_pkg(buf, sizeof(buf) - left, addr));

    return 0;
}

static int handle_udp(struct sockaddr_in *client_addr, void *pkg, int len)
{
    int ret;
    void *p = pkg;
    int left = len;
    struct protocol_head head;

    NEG_RET_LN(get_head(&head, &p, &left));

    if (left == 0) /* logdb reciver return */
    {
        recive_reply_flag = true;

        if (settings.is_return_pkg == true)
        {
            void *data = NULL;
            size_t data_len = 0;

            ret = timer_get(head.sequence, &data_len, &data);
            if (ret < 0)
            {
                log_error("get timer fail: %d", ret);

                return -__LINE__;
            }

            ret = return_to_sender(head.result, data, data_len);
            if (ret < 0)
            {
                log_error("return to sender fail: %d", ret);
                timer_del(head.sequence);

                return -__LINE__;
            }
        }

        if (head.sequence)
        {
            ret = timer_del(head.sequence);
            if (ret < 0)
            {
                log_error("del timer fail: %d, seq: %u", ret, head.sequence);
            }
        }
    }
    else
    {
        uint32_t sequence  = 0;
        void *data = NULL;

        ret = timer_add(sizeof(*client_addr) + len, NULL, handle_time_out, &sequence, &data);
        if (ret < 0)
        {
            log_error("add timer fail: %d", ret);
        }
        else
        {
            memcpy(data, client_addr, sizeof(*client_addr));
            memcpy(data + sizeof(*client_addr), pkg, len);
        }

        ret = send_to_reciver(client_addr, head.command, sequence, p, left);
        if (ret < 0)
        {
            log_error("send to reciver fail: %d", ret);
        }
    }

    return 0;
}

static int pkg_pop(struct sockaddr_in *addr, void **pkg, uint32_t *len)
{
    void *buf;
    uint32_t n;

    int ret = queue_pop(&settings.no_reply_cache_queue, &buf, &n);
    if (ret < 0)
        return ret;

    memcpy(addr, buf, sizeof(*addr));

    *pkg = buf + sizeof(*addr);
    *len = n - sizeof(*addr);

    return 0;
}

static void main_loop(void)
{
    while (true)
    {
        if (shut_down_flag)
        {
            log_vip("interface, shut down...");

            exit(EXIT_SUCCESS);
        }

        struct timeval now;
        gettimeofday(&now, NULL);

        dlog_check(NULL, &now);
        timer_check(&now);

        int ret;
        struct sockaddr_in client_addr;
        void *qpkg = NULL;
        uint32_t qlen = 0;
        static uint32_t seq;

        if (recive_reply_flag == true && (seq++ % 3) == 0)
        {
            ret = pkg_pop(&client_addr, &qpkg, &qlen);
            if (ret < -1)
            {
                log_fatal("queue pop fail: %d", ret);
            }
            else if (ret >= 0)
            {
                ret = handle_udp(&client_addr, qpkg, qlen);
                if (ret < 0)
                {
                    log_error("handle queue pkg fail: %d", ret);
                }
            }
        }

        char pkg[UINT16_MAX];
        int  len = 0;

        ret = recv_udp_pkg(&client_addr, pkg, sizeof(pkg), &len);
        if (ret < -1)
        {
            if (errno)
                log_error("recv udp pkg error: %d: %m", ret);
            else
                log_error("resv dup pkg error: %d", ret);
        }

        if (ret < 0)
            continue;

        ret = handle_udp(&client_addr, pkg, len);
        if (ret < 0)
        {
            log_error("handle udp pkg fail: %d", ret);
        }
    }

    return;
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

    ret = read_settings();
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "read settings from config file %s fail: %d",
                config_file_path, ret);
    }

    char server_name[PATH_MAX];
    snprintf(server_name, sizeof(server_name), "logdb_%s_inf", settings.server_name);

    char path[PATH_MAX] = { 0 };
    chdir(parentpath(realpath(argv[0], path)));

    ret = is_server_exist(server_name);
    if (ret != 0)
    {
        error(EXIT_FAILURE, 0, "server %s may has been exists", settings.server_name);
    }

    /* test udp */
    ret = create_udp_socket(settings.local_ip, settings.listen_port);
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "create udp socket fail: %d", ret);
    }
    close_udp_socket();

    ret = init_log();
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "init log fail: %d", ret);
    }

    ret = init_queue();
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "init queue fail: %d", ret);
    }

    ret = timer_init();
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "init timer fail: %d", ret);
    }

    if (daemon(true, true) < 0)
    {
        error(EXIT_FAILURE, errno, "daemon fail");
    }

    signal(SIGQUIT, handle_signal);
    signal(SIGCHLD, SIG_IGN);

    ret = create_udp_socket(settings.local_ip, settings.listen_port);
    if (ret < 0)
    {
        error(EXIT_FAILURE, errno, "create udp socket fail: %d", ret);
    }

    log_vip("interface start[%d]", getpid());
    printf("%s start[%d]\n", basepath(argv[0]), getpid());

    main_loop();

    return 0;
}

