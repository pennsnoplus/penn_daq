/** \file net_utils.h */

#ifndef __NET_UTILS_H
#define __NET_UTILS_H

#define XL3_PORT 44601
#define SBC_PORT 44630
#define CONT_PORT 44600
#define VIEW_PORT 44599

#define MAX_XL3_CON 19

// fds
int sbc_fd;
int cont_fd;
int view_fd;
int xl3_fd[MAX_XL3_CON];

// locks
int sbc_lock;
int cont_lock;
int view_lock;
int xl3_lock[MAX_XL3_CON];

void setup_sockets();
int bind_socket(char *host, int port);
int printsend(char *fmt, ... );

#endif
