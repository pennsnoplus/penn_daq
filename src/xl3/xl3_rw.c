#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#include "packet_types.h"

#include "daq_utils.h"
#include "net.h"
#include "net_utils.h"

int do_xl3_cmd(XL3_Packet *packet,int xl3num)
{
  if (rw_xl3_fd[xl3num] <= 0){
    pt_printsend("do_xl3_cmd: Invalid crate number (%d): socket value is NULL\n",xl3num);
    return -1;
  }
  
  uint8_t sent_packet_type = packet->cmdHeader.packet_type;
  packet->cmdHeader.packet_num = command_number;
  SwapShortBlock(&(packet->cmdHeader.packet_num),1);

  int n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
  if (n < 0){
    pt_printsend("do_xl3_cmd: Error writing to socket.\n");
    return -1;
  }
  int sent_command_number = command_number;
  command_number++;
  fd_set readable_fdset;
  FD_ZERO(&readable_fdset);
  FD_SET(rw_xl3_fd[xl3num],&readable_fdset);

  struct timeval delay_value;
  delay_value.tv_sec = 4;
  delay_value.tv_usec = 0;
  // lets get a response back from the xl3
  while (1){
    memset(packet,'\0',MAX_PACKET_SIZE);
    int data = select(fdmax+1,&readable_fdset,NULL,NULL,&delay_value);
    // check for errors
    if (data == -1){
      pt_printsend("do_xl3_cmd: Error in select\n");
      return -2;
    }else if (data == 0){
      pt_printsend("do_xl3_cmd: Timeout in select\n");
      return -3;
    }
    // lets see whats readable
    if (FD_ISSET(rw_xl3_fd[xl3num],&readable_fdset)){
      n = recv(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE,0); 
      if (n < 0){
        pt_printsend("do_xl3_cmd: Error receiving data from XL3 #%d. Closing connection\n",xl3num);
        pthread_mutex_lock(&main_fdset_lock);
        FD_CLR(rw_xl3_fd[xl3num],&xl3_fdset);
        FD_CLR(rw_xl3_fd[xl3num],&main_fdset);
        close(rw_xl3_fd[xl3num]);
        xl3_connected[xl3num] = 0;
        rw_xl3_fd[xl3num] = -1;
        pthread_mutex_unlock(&main_fdset_lock);
        return -1;
      }else if (n == 0){
        pt_printsend("Got a zero byte packet, Closing XL3 #%d\n",xl3num);
        pthread_mutex_lock(&main_fdset_lock);
        FD_CLR(rw_xl3_fd[xl3num],&xl3_fdset);
        FD_CLR(rw_xl3_fd[xl3num],&main_fdset);
        close(rw_xl3_fd[xl3num]);
        xl3_connected[xl3num] = 0;
        rw_xl3_fd[xl3num] = -1;
        pthread_mutex_unlock(&main_fdset_lock);
        return -1;
      }

      SwapShortBlock(&(packet->cmdHeader.packet_num),1);
      // if its a message we'll print it out and loop around again
      if (packet->cmdHeader.packet_type == MESSAGE_ID){
        pt_printsend("%s",packet->payload);
      }else if (packet->cmdHeader.packet_type == sent_packet_type){
        // its the right packet type, make sure number is correct
        if (packet->cmdHeader.packet_num == (sent_command_number)%65536){
          return 0;
        }else{
          pt_printsend("do_xl3_cmd(): Got wrong command number back. Expected %d, got %d.\n",sent_command_number,packet->cmdHeader.packet_num);
        }
      }else if (packet->cmdHeader.packet_type == MEGA_BUNDLE_ID){
        //store_mega_bundle();
      }else{
        // got the wrong type of ack packet
        pt_printsend("Got wrong packet type back do_xl3_cmd(): Expected %08x, got %08x\n",sent_packet_type,packet->cmdHeader.packet_type);
      } // end switch on packet type
    }else{
      // ?? its the only thing we're looking at
      pt_printsend("Somehow xl3 still not readable in do_xl3_cmd()\n");
      return -4;
    } // end if xl3 is readable
  }
  return 0;
}

int do_xl3_cmd_no_response(XL3_Packet *packet,int xl3num)
{
  if (rw_xl3_fd[xl3num] <= 0){
    pt_printsend("do_xl3_cmd_no_response: Invalid crate number (%d): socket value is NULL\n",xl3num);
    return -1;
  }
  
  uint8_t sent_packet_type = packet->cmdHeader.packet_type;
  packet->cmdHeader.packet_num = command_number;
  SwapShortBlock(&(packet->cmdHeader.packet_num),1);

  int n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
  if (n < 0){
    pt_printsend("do_xl3_cmd_no_response: Error writing to socket.\n");
    return -1;
  }
  return 0;
}
