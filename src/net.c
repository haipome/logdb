/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/18, create
 */

# include <string.h>
# include <errno.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <sys/time.h>
# include <unistd.h>

static int udp_socket_fd;

int create_udp_socket(const char *local_ip, uint16_t listen_port)
{
    udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_fd < 0)
        return -__LINE__;

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    if (local_ip)
    {
        if (inet_aton(local_ip, &server_addr.sin_addr) == 0)
            return -__LINE__;
    }
    else
    {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    server_addr.sin_port = htons(listen_port);

    if (bind(udp_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        return -__LINE__;

    return 0;
}

int close_udp_socket(void)
{
    return close(udp_socket_fd);
}

int recv_udp_pkg(struct sockaddr_in *client_addr, void *pkg, size_t nbytes, int *pkg_len)
{
    fd_set rset;
    struct timeval timeout;

    FD_ZERO(&rset);
    FD_SET(udp_socket_fd, &rset);

    timeout.tv_sec = 0;
    timeout.tv_usec = 100 * 1000;

    int ret;
    ret = select(udp_socket_fd + 1, &rset, NULL, NULL, &timeout);
    if (ret < 0)
    {
        if (errno == EINTR) /* Interrupted by a signal */
            return -1;

        return -__LINE__;
    }
    else if (ret == 0)
    {
        return -1; /* time out */
    }
    else
    {
        socklen_t addr_len = sizeof(*client_addr);
        *pkg_len = recvfrom(udp_socket_fd, pkg, nbytes, 0, \
                (struct sockaddr *)client_addr, &addr_len);
        if (*pkg_len < 0)
            return -__LINE__;
    }

    return 0;
}

int send_udp_pkg(void *pkg, size_t len, struct sockaddr_in *addr)
{
    return sendto(udp_socket_fd, pkg, len, 0, (struct sockaddr *)addr, sizeof(*addr));
}

