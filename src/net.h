/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/06/18, create
 */

# pragma once

# include <netinet/in.h>

int create_udp_socket(const char *local_ip, uint16_t listen_port);
int close_udp_socket(void);

int recv_udp_pkg(struct sockaddr_in *client_addr, void *pkg, size_t nbytes, int *pkg_len);

int send_udp_pkg(void *pkg, size_t len, struct sockaddr_in *addr);

