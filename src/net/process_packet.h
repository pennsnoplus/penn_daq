/** \file process_packet.h */

#ifndef __PROCESS_PACKET_H
#define __PROCESS_PACKET_H

long int start_time,end_time,last_print_time;
long int recv_bytes, recv_fake_bytes;
int megabundle_count;

int read_xl3_packet(int fd);
int read_control_command(int fd);
int read_viewer_data(int fd);
int process_control_command(char *buffer);
int process_xl3_packet(char *buffer, int xl3num);
int handle_error_packet(XL3_Packet *packet, int xl3num);
int handle_screwed_packet(XL3_Packet *packet, int xl3num);

int inspect_bundles(XL3_Packet *packet);

#endif


