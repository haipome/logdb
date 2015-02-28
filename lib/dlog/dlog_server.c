/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/09/25, create
 */

# ifndef _GNU_SOURCE
# define _GNU_SOURCE
# endif

# ifndef DLOG_SERVER
# define DLOG_SERVER
# endif

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <strings.h>
# include <stdint.h>
# include <stdbool.h>
# include <signal.h>
# include <ctype.h>
# include <limits.h>
# include <unistd.h>
# include <time.h>
# include <sys/time.h>
# include <error.h>
# include <errno.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/select.h>
# include <getopt.h>

# include "dlog.h"

void print_help(void)
{
    printf("Usage:\n");
    printf("\t-h --help\n\n");
    printf("\t-b --base base_name\n");
    printf("\t-t --type shift_type(size/day/hour/min)\n");
    printf("\t-s --size max_size\n");
    printf("\t-n --num  log_num\n");
    printf("\t-k --keep keep_time\n\n");
    printf("\t-p --port port\n");
    printf("\t-a --addr\n");
    printf("\t-d --daemon\n\n");
    printf("<damonyang@tencent.com>\n");
}

int running;
void sig_handle(int signo)
{
    switch (signo)
    {
        case SIGQUIT:
            running = 0;
            break;
    }
}

int strtotype(char *type)
{
    size_t len = strlen(type);
    int i;
    for (i = 0; i < len; ++i)
        type[i] = tolower(type[i]);

    if (strcmp(type, "size") == 0)
        return DLOG_SHIFT_BY_SIZE;
    else if (strcmp(type, "day") == 0)
        return DLOG_SHIFT_BY_DAY;
    else if (strcmp(type, "hour") == 0)
        return DLOG_SHIFT_BY_HOUR;
    else if (strcmp(type, "min") == 0)
        return DLOG_SHIFT_BY_MIN;

    return -1;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        print_help();
        exit(1);
    }

    opterr = 1;
    int option_index = 0;
    struct option long_options[] =
    {
        {"help",    no_argument,       NULL, 'h'},
        {"base",    required_argument, NULL, 'b'},
        {"type",    required_argument, NULL, 't'},
        {"size",    required_argument, NULL, 's'},
        {"num",     required_argument, NULL, 'n'},
        {"keep",    required_argument, NULL, 'k'},
        {"port",    required_argument, NULL, 'p'},
        {"addr",    no_argument,       NULL, 'a'},
        {"daemon",  no_argument,       NULL, 'd'},
        {0, 0, 0, 0},
    };

    char *base_name = NULL;
    int shift_type  = 0;
    size_t max_size = 0;
    int log_num     = 0;
    int keep_time   = 0;
    int port        = 0;
    bool log_addr   = 0;
    bool in_daemon  = 0;

    int c;
    while ((c = getopt_long(argc, argv, "hb:t:s:n:k:p:ad", long_options, &option_index)) != -1)
    {
        switch (c)
        {
            case 'h':
                print_help();
                exit(0);
            case 'b':
                base_name = strdup(optarg);
                break;
            case 't':
                if ((shift_type = strtotype(optarg)) < 0)
                    error(1, 0, "invalid shift type");
                break;
            case 's':
                max_size = strtoull(optarg, NULL, 0);
                break;
            case 'n':
                log_num = atoi(optarg);
                break;
            case 'k':
                keep_time = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'a':
                log_addr = true;
                break;
            case 'd':
                in_daemon = true;
                break;
            case '?':
                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }

    if (base_name == NULL)
        error(1, errno, "-b --base base_name is required");
    if (port == 0)
        error(1, errno, "-p --port port is required");
    if (shift_type == 0)
        shift_type = DLOG_SHIFT_BY_DAY;

    if (in_daemon)
        daemon(true, true);

    dlog_t *lp = dlog_init(base_name, shift_type | DLOG_USE_FORK, max_size, log_num, keep_time);
    if (lp == NULL)
        error(1, errno, "dlog_init fail");

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error(1, errno, "create socket fail");

    struct sockaddr_in server;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons((uint16_t)port);

    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
        error(1, errno, "bind fail");

    running = 1;

    signal(SIGQUIT, sig_handle);
    signal(SIGCHLD, SIG_IGN);

    while (running)
    {
        fd_set r_set;
        FD_ZERO(&r_set);
        FD_SET(sockfd, &r_set);

        int ret = select(sockfd + 1, &r_set, NULL, NULL, NULL);
        if (ret > 0 && FD_ISSET(sockfd, &r_set))
        {
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            
            char buf[UINT16_MAX];
            ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&client, &len);
            if (n < 0)
                continue;

            dlog_server(lp, buf, n, log_addr ? &client : NULL);
        }
    }

    return 0;
}

