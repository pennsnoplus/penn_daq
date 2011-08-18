#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

#include "net_utils.h"

void setup_sockets()
{
  int i;
  cont_fd = bind_socket("0.0.0.0", CONT_PORT);
  sbc_fd = bind_socket("0.0.0.0", SBC_PORT);
  view_fd = bind_socket("0.0.0.0", VIEW_PORT);
  for (i=0;i<MAX_XL3_CON;i++){
    xl3_fd[i] = bind_socket("0.0.0.0",XL3_PORT+i);
  }
}

int bind_socket(char *host, int port)
{
  int rv, listener;
  int yes = 1;
  char str_port[10];
  sprintf(str_port,"%d",port);
  struct addrinfo hints, *ai, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((rv = getaddrinfo(host, str_port, &hints, &ai)) != 0) {

    printsend( "Bad address info for port %d: %s\n",port,gai_strerror(rv));
    return -1;
    sigint_func(SIGINT);
  }
  for(p = ai; p != NULL; p = p->ai_next) {
    listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listener < 0) {
      continue;
    }
    // lose the pesky "address already in use" error message
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
      close(listener);
      continue;
    }
    break;
  }
  if (p == NULL) {
    // If p == NULL, that means that listener
    // did not get bound to the port

    printsend( "Failed to bind to socket for port %d\n",port);
    return -1;
  }
  else{
    return listener;
  }
  freeaddrinfo(ai); // all done with this
}

int printsend(char *fmt, ... ) //FIXME
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg, fmt);
  ret = vsprintf(psb,fmt, arg);
  fputs(psb, stdout);
  /*
     fd_set outset;
     FD_ZERO(&outset);

     int i, count=0;
     for(i = 0; i <= fdmax; i++){
     if (FD_ISSET(i, &view_fdset)){
     count++;
     }
     }

     int select_return, x;
     if(count > 1){ // always 1: view listener
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
  */
  return ret;
}

