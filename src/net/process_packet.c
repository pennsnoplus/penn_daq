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
#include "mtc_utils.h"
#include "net_utils.h"
#include "net.h"
#include "process_packet.h"

int read_xl3_data(int fd)
{
  int i,crate;
  for (i=0;i<MAX_XL3_CON;i++){
    if (fd == rw_xl3_fd[i])
      crate = i;
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
    xl3_connected[i] = 0;
    rw_xl3_fd[i] = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Got a zero byte packet, Closing XL3 #%d\n",crate);
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    xl3_connected[i] = 0;
    rw_xl3_fd[i] = -1;
    return -1;
  }
  // otherwise, process the packet
  // process_xl3_data();  
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
  }else if (strncmp(buffer,"start_logging",13)==0){
    result = start_logging();
  }else if (strncmp(buffer,"debugging_on",12)==0){
    result = debugging_mode(buffer,1);
  }else if (strncmp(buffer,"debugging_off",13)==0){
    result = debugging_mode(buffer,0);
  }else if (strncmp(buffer,"set_location",12)==0){
    result = set_location(buffer);
  }else if (strncmp(buffer,"change_mode",11)==0){
    result = change_mode(buffer);
  }else if (strncmp(buffer,"crate_init",10)==0){
    result = crate_init(buffer);
  }else if (strncmp(buffer,"sbc_control",11)==0){
    result = sbc_control(buffer);
  }
  //_!_end_commands_!_
  else
    printsend("not a valid command\n");
  return result;
}


