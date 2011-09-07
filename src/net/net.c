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
#include "process_packet.h"
#include "net.h"

void setup_sockets()
{
  int i;
  // lets bind some sockets
  listen_cont_fd = bind_socket("0.0.0.0", CONT_PORT);
  listen_view_fd = bind_socket("0.0.0.0", VIEW_PORT);
  for (i=0;i<MAX_XL3_CON;i++)
    listen_xl3_fd[i] = bind_socket("0.0.0.0",XL3_PORT+i);

  // now we can set them up to listen for incoming connections
  if (listen(listen_cont_fd,MAX_PENDING_CONS) == - 1){
    printf("Problem setting up socket to listen for controllers\n");
    sigint_func(SIGINT);
  }
  if (listen(listen_view_fd,MAX_PENDING_CONS) == - 1){
    printf("Problem setting up socket to listen for viewers\n");
    sigint_func(SIGINT);
  }
  for (i=0;i<MAX_XL3_CON;i++)
    if (listen(listen_xl3_fd[i],MAX_PENDING_CONS) == - 1){
      printf("Problem setting up socket to listen for xl3 #%d\n",i);
      sigint_func(SIGINT);
    }

  // now find the max socket number
  fdmax = listen_cont_fd;
  if (listen_view_fd > fdmax)
    fdmax = listen_view_fd;
  for (i=0;i<MAX_XL3_CON;i++)
    if (listen_xl3_fd[i] > fdmax)
      fdmax = listen_xl3_fd[i];

  // set up some fdsets
  FD_ZERO(&main_fdset);
  FD_ZERO(&main_readable_fdset);
  FD_ZERO(&main_writeable_fdset);
  FD_ZERO(&new_connection_fdset);
  FD_ZERO(&view_fdset);
  FD_ZERO(&cont_fdset);
  FD_ZERO(&xl3_fdset);

  FD_SET(listen_view_fd,&main_fdset);
  FD_SET(listen_cont_fd,&main_fdset);
  for (i=0;i<MAX_XL3_CON;i++)
    FD_SET(listen_xl3_fd[i],&main_fdset);

  // zero some stuff
  rw_cont_fd = -1;
  for (i=0;i<MAX_VIEW_CON;i++)
    rw_view_fd[i] = -1;
  for (i=0;i<MAX_XL3_CON;i++)
    rw_xl3_fd[i] = -1;
  rw_sbc_fd = -1;

  // set up flags
  sbc_connected = 0;
  cont_connected = 0;
  views_connected = 0;
  for (i=0;i<MAX_XL3_CON;i++)
    xl3_connected[i] = 0;

  // set up locks
  sbc_lock = 0;
  for (i=0;i<MAX_XL3_CON;i++)
    xl3_lock[i] = 0;

  new_connection_fdset = main_fdset;
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

void read_socket(int fd)
{
  // check what kind of socket we are reading from
  if (FD_ISSET(fd, &new_connection_fdset))
    accept_connection(fd);
  else if (FD_ISSET(fd, &xl3_fdset))
    read_xl3_packet(fd);
  else if (FD_ISSET(fd, &cont_fdset))
    read_control_command(fd);
  else if (FD_ISSET(fd, &view_fdset))
    read_viewer_data(fd);
}

int accept_connection(int fd)
{
  pthread_mutex_lock(&main_fdset_lock);
  struct sockaddr_storage remoteaddr;
  socklen_t addrlen;
  char remoteIP[INET6_ADDRSTRLEN]; // character array to hold the remote IP address
  addrlen = sizeof remoteaddr;
  int new_fd;
  int i;
  char rejectmsg[100];
  memset(rejectmsg,'\0',sizeof(rejectmsg));

  // accept the connection
  new_fd = accept(fd, (struct sockaddr *) &remoteaddr, &addrlen);
  if (new_fd == -1){
    printsend("Error accepting a new connection.\n");
    close (new_fd);
    return -1;
  }else if (new_fd == 0){
    printsend("Failed accepting a new connection.\n");
    close (new_fd);
    return -1;
  }

  // now check which socket type it was
  if (fd == listen_cont_fd){
    if (cont_connected){
      printsend("Another controller tried to connect and was rejected.\n");
      sprintf(rejectmsg,"Too many controller connections already. Goodbye.\n");
      send(new_fd,rejectmsg,sizeof(rejectmsg),0);
      close(new_fd);
      return 0;
    }else{
      printsend("New connection: CONTROLLER (%s) on socket %d\n",
          inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
            remoteIP, INET6_ADDRSTRLEN), new_fd);
      cont_connected = 1;
      rw_cont_fd = new_fd;
      FD_SET(new_fd,&cont_fdset);
    }
  }else if (fd == listen_view_fd){
    if (views_connected >= MAX_VIEW_CON){
      printsend("Another viewer tried to connect and was rejected.\n");
      sprintf(rejectmsg,"Too many viewer connections already. Goodbye.\n");
      send(new_fd,rejectmsg,sizeof(rejectmsg),0);
      close(new_fd);
      return 0;
    }else{
      printsend("New connection: VIEWER (%s) on socket %d\n",
          inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr),
            remoteIP,INET6_ADDRSTRLEN), new_fd);
      views_connected++;
      FD_SET(new_fd,&view_fdset);
      for (i=0;i<MAX_VIEW_CON;i++)
        if (rw_view_fd[i] == -1){
          rw_view_fd[i] = new_fd;
          break;
        }

    }
  }else{
    int i;
    for (i=0;i<MAX_XL3_CON;i++){
      if (fd == listen_xl3_fd[i]){
        if (xl3_connected[i]){
          // xl3s dont close their connections,
          // so we assume this means its reconnecting
          printsend("Going to reconnect.\n");
          if (rw_xl3_fd[i] != new_fd){
            // dont close it if its the same socket we
            // just reopened on!
            printsend("Closing old XL3 #%d connection on socket %d\n",i,rw_xl3_fd[i]);
            close(rw_xl3_fd[i]);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
          }
        }
        printsend("New connection: XL3 #%d on socket %d\n",i,new_fd); 
        rw_xl3_fd[i] = new_fd;
        xl3_connected[i] = 1;
        FD_SET(new_fd,&xl3_fdset);
      }
    } 
  } // end switch over which socket type

  if (new_fd > fdmax)
    fdmax = new_fd;
  FD_SET(new_fd,&main_fdset);
  pthread_mutex_unlock(&main_fdset_lock);
}

