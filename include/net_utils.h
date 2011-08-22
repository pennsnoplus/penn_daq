/** \file net_utils.h */

#ifndef __NET_UTILS_H
#define __NET_UTILS_H

#include <sys/socket.h>

#include "main.h"

#define XL3_PORT 44601
#define SBC_PORT 44630
#define CONT_PORT 44600
#define VIEW_PORT 44599

#define MAX_XL3_CON 19
#define MAX_VIEW_CON 3

#define MAX_PENDING_CONS 20

#define CONT_CMD_ACK "_!_"

// listening fds
int listen_cont_fd;
int listen_view_fd;
int listen_xl3_fd[MAX_XL3_CON];

// read/write fds
int rw_cont_fd;
int rw_view_fd[MAX_VIEW_CON];
int rw_xl3_fd[MAX_XL3_CON];
int rw_sbc_fd;


//fdsets
fd_set main_fdset;
fd_set main_readable_fdset;
fd_set main_writeable_fdset;
fd_set new_connection_fdset;
fd_set view_fdset;
fd_set cont_fdset;
fd_set xl3_fdset;
int fdmax;

// locks
int sbc_lock;
int cont_lock;
int view_lock;
int xl3_lock[MAX_XL3_CON];

// connection flag
int sbc_connected;
int cont_connected;
int views_connected;
int xl3_connected[MAX_XL3_CON];

char buffer[MAX_PACKET_SIZE];

void read_socket(int fd);
int accept_connection(int fd);
void setup_sockets();
int bind_socket(char *host, int port);
int printsend(char *fmt, ... );
void *get_in_addr(struct sockaddr *sa);
int read_xl3_data(int fd);
int read_control_command(int fd);
int read_viewer_data(int fd);
int process_control_command(char *buffer);
int print_connected();

#endif
