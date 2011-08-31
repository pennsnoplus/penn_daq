#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include "packet_types.h"

#include "main.h"
#include "xl3_utils.h"
#include "xl3_rw.h"
#include "xl3_cmds.h"
#include "mtc_utils.h"
#include "mtc_cmds.h"
#include "mtc_init.h"
#include "crate_init.h"
#include "net_utils.h"
#include "net.h"
#include "fec_test.h"
#include "process_packet.h"

int read_xl3_packet(int fd)
{
  int i,crate = -1;
  for (i=0;i<MAX_XL3_CON;i++){
    if (fd == rw_xl3_fd[i])
      crate = i;
  }
  if (crate == -1){
    printsend("Incorrect xl3 fd? No matching crate\n");
    return -1;
  }
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving data from XL3 #%d\n",crate);
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    xl3_connected[crate] = 0;
    rw_xl3_fd[crate] = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Got a zero byte packet, Closing XL3 #%d\n",crate);
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    xl3_connected[crate] = 0;
    rw_xl3_fd[crate] = -1;
    return -1;
  }
  // otherwise, process the packet
  process_xl3_packet(buffer,crate);  
  return 0;
}

int read_control_command(int fd)
{
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving command from controller\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&cont_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    cont_connected = 0;
    rw_cont_fd = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Closing controller connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&cont_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    cont_connected = 0;
    rw_cont_fd = -1;
    return -1;
  }
  // otherwise process the packet
  int error = process_control_command(buffer);
  // send response
  if (error != 0){
    // one of the sockets was locked already or no threads available
    if (FD_ISSET(fd,&main_writeable_fdset))
      write(fd,CONT_CMD_BSY,strlen(CONT_CMD_BSY));
    else
      printsend("Could not send response to controller - check connection\n");
  }else{
    // command was processed successfully
    if (FD_ISSET(fd,&main_writeable_fdset))
      write(fd,CONT_CMD_ACK,strlen(CONT_CMD_ACK));
    else
      printsend("Could not send response to controller - check connection\n");
  }
  return 0;
}

int read_viewer_data(int fd)
{
  int i;
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving packet from viewer\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&view_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    for (i=0;i<views_connected;i++)
      if (rw_view_fd[i] == fd)
        rw_view_fd[i] = -1;
    views_connected--;
    return -1;
  }else if (numbytes == 0){
    printsend("Closing viewer connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&view_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    for (i=0;i<views_connected;i++)
      if (rw_view_fd[i] == fd)
        rw_view_fd[i] = -1;
    views_connected--;
    return -1;
  }

}

int process_control_command(char *buffer)
{
  int result = 0;
  //_!_begin_commands_!_
  if (strncmp(buffer,"exit",4)==0){
    printsend("Exiting daq.\n");
    sigint_func(SIGINT);
  }else if (strncmp(buffer,"print_connected",10)==0){
    result = print_connected();
  }else if (strncmp(buffer,"stop_logging",12)==0){
    result = stop_logging();
  }else if (strncmp(buffer,"set_location",12)==0){
    result = set_location(buffer);
  }else if (strncmp(buffer,"start_logging",13)==0){
    result = start_logging();
  }else if (strncmp(buffer,"sbc_control",11)==0){
    result = sbc_control(buffer);
  }else if (strncmp(buffer,"clear_screen",12)==0){
    system("clear");
    pt_printsend("Cleared screen\n");
  }else if (strncmp(buffer,"debugging_on",12)==0){
    result = debugging_mode(buffer,1);
  }else if (strncmp(buffer,"debugging_off",13)==0){
    result = debugging_mode(buffer,0);
  }else if (strncmp(buffer,"change_mode",11)==0){
    result = change_mode(buffer);
  }else if (strncmp(buffer,"crate_init",10)==0){
    result = crate_init(buffer);
  }else if (strncmp(buffer,"mtc_init",8)==0){
    result = mtc_init(buffer);
  }else if (strncmp(buffer,"fec_test",8)==0){
    result = fec_test(buffer);
  }else if (strncmp(buffer,"sm_reset",8)==0){
    result = sm_reset(buffer);
  }else if (strncmp(buffer,"xl3_rw",6)==0){
    result = cmd_xl3_rw(buffer);
  }else if (strncmp(buffer,"xl3_queue_rw",13)==0){
    result = xl3_queue_rw(buffer);
  }else if (strncmp(buffer,"read_bundle",11)==0){
    result = read_bundle(buffer);
  }else if (strncmp(buffer,"mtc_read",8)==0){
    result = mtc_read(buffer);
  }else if (strncmp(buffer,"mtc_write",9)==0){
    result = mtc_write(buffer);
  }else if (strncmp(buffer,"set_mtca_thresholds",14)==0){
    result = set_mtca_thresholds(buffer);
  }else if (strncmp(buffer,"set_gt_mask",11)==0){
    result = cmd_set_gt_mask(buffer);
  }else if (strncmp(buffer,"set_gt_crate_mask",17)==0){
    result = cmd_set_gt_crate_mask(buffer);
  }else if (strncmp(buffer,"set_ped_crate_mask",18)==0){
    result = cmd_set_ped_crate_mask(buffer);
  }
  //_!_end_commands_!_
  else
    printsend("not a valid command\n");
  return result;
}

int process_xl3_packet(char *buffer, int xl3num)
{
  XL3_Packet *packet = (XL3_Packet *) buffer;
  SwapShortBlock(&(packet->cmdHeader.packet_num),1);
  // if its a message we'll print it out and loop around again
  if (packet->cmdHeader.packet_type == MESSAGE_ID){
    pt_printsend("%s",packet->payload);
  }else if (packet->cmdHeader.packet_type == MEGA_BUNDLE_ID){
    //store_mega_bundle();
  }else if (packet->cmdHeader.packet_type == PING_ID){
    packet->cmdHeader.packet_type = PONG_ID;
    int n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
    if (n < 0){
      pt_printsend("process_xl3_packet: Error writing to socket for pong.\n");
      return -1;
    }
  }else{
    // got the wrong type of ack packet
    pt_printsend("process_xl3_packet: Got unexpected packet type in main loop: %08x\n",packet->cmdHeader.packet_type);
  } // end switch on packet type
  return 0;
}

