/** \file process_packet.h */

#ifndef __PROCESS_PACKET_H
#define __PROCESS_PACKET_H

int read_xl3_data(int fd);
int read_control_command(int fd);
int read_viewer_data(int fd);
int process_control_command(char *buffer);

#endif


