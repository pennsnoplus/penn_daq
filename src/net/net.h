/** \file net.h */

#ifndef __NET_H
#define __NET_H

#include <sys/socket.h>
#include <pthread.h>

#include "packet_types.h"

#include "main.h"

#define MAX_PENDING_CONS 20

//XL3
#define XL3_PORT 44601
#define MAX_XL3_CON 19

//SBC
#define SBC_PORT 44630
#define SBC_USER "daq"
#define SBC_SERVER "10.0.0.30"

//CONTROLLER
#define CONT_PORT 44600
#define CONT_CMD_ACK "_._"
#define CONT_CMD_BSY "_!_"

//VIEWER
#define VIEW_PORT 44599
#define MAX_VIEW_CON 3

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
pthread_mutex_t main_fdset_lock;

// locks
int sbc_lock;
int xl3_lock[MAX_XL3_CON];

// connection flag
int sbc_connected;
int cont_connected;
int views_connected;
int xl3_connected[MAX_XL3_CON];

char buffer[MAX_PACKET_SIZE];


// function prototypes
void setup_sockets();
int bind_socket(char *host, int port);
void read_socket(int fd);
int accept_connection(int fd);

#endif
