/** \file net_utils.h */

#ifndef __NET_UTILS_H
#define __NET_UTILS_H

#include <sys/socket.h>
#include <pthread.h>

char printsend_buffer[10000];
pthread_mutex_t printsend_buffer_lock;

int printsend(char *fmt, ... );
int pt_printsend(char *fmt, ...);
int send_queued_msgs();
int print_connected();
void *get_in_addr(struct sockaddr *sa);

#endif

