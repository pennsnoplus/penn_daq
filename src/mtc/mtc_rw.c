#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <signal.h>

#include <string.h>
#include <stdlib.h> 
#include <stdio.h>

#include "net.h"
#include "net_utils.h"
#include "mtc_rw.h"


int do_mtc_xilinx_cmd(SBC_Packet packet)
{
  if(rw_sbc_fd <= 0){
    pt_printsend("do_mtc_xilinx_cmd: not connected to the MTC/SBC\n");
    return -1;
  }

  int32_t numBytesToSend = packet.numBytes;
  //char* p = (char*)packet;
  SBC_Packet bPacket;
  char* q = (char*)&bPacket;

  pt_printsend("going to write xilinx to mtc, %d bytes\n",numBytesToSend); //REMOVE
  //numBytesToSend = 1024;
  int n = write(rw_sbc_fd,(char *)&packet,sizeof(packet));
  printf("n was %d\n",n);
  pt_printsend("wrote xilinx to mtc\n");
  if (n < 0) {
    pt_printsend("do_mtc_xilinx_cmd: ERROR writing to socket\n");
    return -1;
  }

  fd_set temp_fdset;
  FD_ZERO(&temp_fdset);
  FD_SET(rw_sbc_fd,&temp_fdset);

  struct timeval delay_value;
  delay_value.tv_sec = 4;
  delay_value.tv_usec = 0;
  pt_printsend("going to select\n"); //REMOVE
  int data=select(fdmax+1, &temp_fdset, NULL, NULL, &delay_value);
  pt_printsend("selected\n"); //REMOVE
  if (data == -1){
    pt_printsend("do_mtc_xilinx_cmd: Error in select\n");
    return -2;
  }else if (data == 0){
    pt_printsend("do_mtc_xilinx_cmd: Timeout in select\n");
    return -3;
  }

  do{
    pt_printsend("going to recv\n"); //REMOVE
    n = recv(rw_sbc_fd,(char *)&packet,numBytesToSend+10, 0 ); // 1500 should be?
    pt_printsend("recvd\n"); //REMOVE
    numBytesToSend-=n;
    if (n < 0){
      pt_printsend("do_mtc_xilinx_cmd: Error receiving data from sbc. Closing connection.\n");
      pthread_mutex_lock(&main_fdset_lock);
      FD_CLR(rw_sbc_fd,&main_fdset);
      close(rw_sbc_fd);
      sbc_connected = 0;
      rw_sbc_fd = -1;
      pthread_mutex_unlock(&main_fdset_lock);
      return -1;
    }else if (n == 0){
      pt_printsend("do_mtc_xilinx_cmd: Got a zero byte packet, Closing sbc connection\n");
      pthread_mutex_lock(&main_fdset_lock);
      FD_CLR(rw_sbc_fd,&main_fdset);
      close(rw_sbc_fd);
      sbc_connected = 0;
      rw_sbc_fd = -1;
      pthread_mutex_unlock(&main_fdset_lock);
      return -1;
    }
  }while(numBytesToSend>0);

  return 0;
} 
