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
#include "net.h"
#include "net_utils.h"

int print_connected()
{
  int i,y = 0;
  printsend("CONNECTED CLIENTS:\n");

  if (cont_connected){
    printsend("\t Controller (socket: %d)\n",rw_cont_fd);
    y++;
  }
  for (i=0;i<views_connected;i++){
    if (rw_view_fd[i] != -1){
      printsend("\t Viewer (socket: %d)\n",rw_view_fd[i]);
      y++;
    }
  }
  if (sbc_connected){
    printsend("\t SBC (socket: %d)\n",rw_sbc_fd);
    y++;
  }
  for (i=0;i<MAX_XL3_CON;i++){
    if (xl3_connected[i]){
      printsend("\t XL3 #%d (socket: %d)\n",i,rw_xl3_fd[i]);
      y++;
    }
  }
  if (y == 0)
    printsend("\t No connected boards\n");
  return 0;
}

int send_queued_msgs()
{
  pthread_mutex_lock(&printsend_buffer_lock);
  if (strlen(printsend_buffer) > 0){
    printsend("%s",printsend_buffer);
    memset(printsend_buffer,'\0',sizeof(printsend_buffer));
  }
  pthread_mutex_unlock(&printsend_buffer_lock);

}

int pt_printsend(char *fmt, ...)
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg, fmt);
  ret = vsprintf(psb,fmt, arg);
  printf("%s",psb);
  //pthread_mutex_lock(&printsend_buffer_lock);
  //sprintf(printsend_buffer+strlen(printsend_buffer),"%s",psb);
  //pthread_mutex_unlock(&printsend_buffer_lock);
  return 0;
}

int printsend(char *fmt, ... )
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg, fmt);
  ret = vsprintf(psb,fmt, arg);
  fputs(psb, stdout);
  fd_set outset;
  FD_ZERO(&outset);

  int i, count=0;
  for(i = 0; i <= fdmax; i++){
    if (FD_ISSET(i, &view_fdset)){
      count++;
    }
  }

  int select_return, x;
  if(count > 0){
    outset = view_fdset;
    select_return = select(fdmax+1, NULL, &outset, NULL, 0);
    // if there were writeable file descriptors
    if(select_return > 0){
      for(x = 0; x <= fdmax; x++){
        if(FD_ISSET(x, &outset)){
          write(x, psb, ret);
        }
      }
    }
  }
  if (write_log && ps_log_file){
    fprintf(ps_log_file, "%s", psb);
  }
  return ret;
}

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
