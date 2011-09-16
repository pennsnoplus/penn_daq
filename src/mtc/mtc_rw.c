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

int do_mtc_cmd(SBC_Packet *packet)
{
  if(rw_sbc_fd <= 0){
    pt_printsend("do_mtc_cmd: not connected to the MTC/SBC\n");
    return -1;
  }
  packet->numBytes = packet->cmdHeader.numberBytesinPayload + sizeof(uint32_t) +
    sizeof(SBC_CommandHeader) + kSBC_MaxMessageSizeBytes;
  int32_t numBytesToSend = packet->numBytes;
  int n = write(rw_sbc_fd,(char *)packet,numBytesToSend);
  if (n < 0) {
    pt_printsend("do_mtc_cmd: ERROR writing to socket\n");
    return -1;
  }

  fd_set temp_fdset;
  FD_ZERO(&temp_fdset);
  FD_SET(rw_sbc_fd,&temp_fdset);

  struct timeval delay_value;
  delay_value.tv_sec = 4;
  delay_value.tv_usec = 0;
  int data=select(fdmax+1, &temp_fdset, NULL, NULL, &delay_value);
  if (data == -1){
    pt_printsend("do_mtc_cmd: Error in select\n");
    return -2;
  }else if (data == 0){
    pt_printsend("do_mtc_cmd: Timeout in select\n");
    return -3;
  }

  n = recv(rw_sbc_fd,(char *)packet,numBytesToSend, 0 ); // 1500 should be?
  if (n < 0){
    pt_printsend("do_mtc_cmd: Error receiving data from sbc. Closing connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(rw_sbc_fd,&main_fdset);
    close(rw_sbc_fd);
    rw_sbc_fd = -1;
    pthread_mutex_unlock(&main_fdset_lock);
    return -1;
  }else if (n == 0){
    pt_printsend("do_mtc_cmd: Got a zero byte packet, Closing sbc connection\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(rw_sbc_fd,&main_fdset);
    close(rw_sbc_fd);
    rw_sbc_fd = -1;
    pthread_mutex_unlock(&main_fdset_lock);
    return -1;
  }

  return 0;
} 

int do_mtc_xilinx_cmd(SBC_Packet *packet)
{
  if(rw_sbc_fd <= 0){
    pt_printsend("do_mtc_xilinx_cmd: not connected to the MTC/SBC\n");
    return -1;
  }

  int32_t numBytesToSend = packet->numBytes;

  //numBytesToSend = 1024;
  int n = write(rw_sbc_fd,(char *)packet,numBytesToSend);
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
  int data=select(fdmax+1, &temp_fdset, NULL, NULL, &delay_value);
  if (data == -1){
    pt_printsend("do_mtc_xilinx_cmd: Error in select\n");
    return -2;
  }else if (data == 0){
    pt_printsend("do_mtc_xilinx_cmd: Timeout in select\n");
    return -3;
  }

  int bytes_recvd = 0;
  do{
    n = recv(rw_sbc_fd,(char *)packet+bytes_recvd,numBytesToSend+10, 0 ); // 1500 should be?
    bytes_recvd+=n;
    if (n < 0){
      pt_printsend("do_mtc_xilinx_cmd: Error receiving data from sbc. Closing connection.\n");
      pthread_mutex_lock(&main_fdset_lock);
      FD_CLR(rw_sbc_fd,&main_fdset);
      close(rw_sbc_fd);
      rw_sbc_fd = -1;
      pthread_mutex_unlock(&main_fdset_lock);
      return -1;
    }else if (n == 0){
      pt_printsend("do_mtc_xilinx_cmd: Got a zero byte packet, Closing sbc connection\n");
      pthread_mutex_lock(&main_fdset_lock);
      FD_CLR(rw_sbc_fd,&main_fdset);
      close(rw_sbc_fd);
      rw_sbc_fd = -1;
      pthread_mutex_unlock(&main_fdset_lock);
      return -1;
    }
  }while(bytes_recvd<numBytesToSend);

  return 0;
} 

int mtc_reg_write(uint32_t address, uint32_t data)
{
  SBC_Packet *packet;
  packet = malloc(sizeof(SBC_Packet));
  memset(packet,0,sizeof(SBC_Packet));
  packet->cmdHeader.destination = 0x1;
  packet->cmdHeader.cmdID = MTC_WRITE_ID;
  packet->cmdHeader.numberBytesinPayload  = sizeof(SBC_VmeWriteBlockStruct)+sizeof(uint32_t);
  //packet->cmdHeader.numberBytesinPayload  = 256+28;
  //packet->numBytes = 256+28+16;
  SBC_VmeWriteBlockStruct *writestruct;
  writestruct = (SBC_VmeWriteBlockStruct *) packet->payload;
  writestruct->address = address + MTCRegAddressBase;
  writestruct->addressModifier = MTCRegAddressMod;
  writestruct->addressSpace = MTCRegAddressSpace;
  writestruct->unitSize = 4;
  writestruct->numItems = 1;
  writestruct++;
  uint32_t *data_ptr = (uint32_t *) writestruct;
  *data_ptr = data;

  do_mtc_cmd(packet);
  free(packet);
  return 0;
}

int mtc_reg_read(uint32_t address, uint32_t *data)
{
  SBC_Packet *packet;
  packet = malloc(sizeof(SBC_Packet));
  memset(packet,0,sizeof(SBC_Packet));
  uint32_t *result;
  packet->cmdHeader.destination = 0x1;
  packet->cmdHeader.cmdID = MTC_READ_ID;
  packet->cmdHeader.numberBytesinPayload = sizeof(SBC_VmeReadBlockStruct)+sizeof(uint32_t);
  //packet->numBytes = 256+27+16;
  SBC_VmeReadBlockStruct *readstruct;
  readstruct = (SBC_VmeReadBlockStruct *) packet->payload;
  readstruct->address = address + MTCRegAddressBase;
  readstruct->addressModifier = MTCRegAddressMod;
  readstruct->addressSpace = MTCRegAddressSpace;
  readstruct->unitSize = 4;
  readstruct->numItems = 1;

  do_mtc_cmd(packet);
  result = (uint32_t *) (readstruct+1);
  *data = *result;
  free(packet);
  return 0;
}


