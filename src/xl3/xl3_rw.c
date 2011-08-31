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
  packet->cmdHeader.packet_num = command_number[xl3num];
  SwapShortBlock(&(packet->cmdHeader.packet_num),1);

  int n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
  if (n < 0){
    pt_printsend("do_xl3_cmd: Error writing to socket.\n");
    return -1;
  }
  int sent_command_number = command_number[xl3num];
  command_number[xl3num]++;
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
      }else if (packet->cmdHeader.packet_type == MEGA_BUNDLE_ID){
        //store_mega_bundle();
      }else if (packet->cmdHeader.packet_type == PING_ID){
        packet->cmdHeader.packet_type = PONG_ID;
        n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
        if (n < 0){
          pt_printsend("do_xl3_cmd: Error writing to socket for pong.\n");
          return -1;
        }
      }else if (packet->cmdHeader.packet_type == sent_packet_type){
        // its the right packet type, make sure number is correct
        if (packet->cmdHeader.packet_num == (sent_command_number)%65536){
          return 0;
        }else{
          pt_printsend("do_xl3_cmd(): Got wrong command number back. Expected %d, got %d.\n",sent_command_number,packet->cmdHeader.packet_num);
        }
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
  packet->cmdHeader.packet_num = command_number[xl3num];
  SwapShortBlock(&(packet->cmdHeader.packet_num),1);

  int n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
  if (n < 0){
    pt_printsend("do_xl3_cmd_no_response: Error writing to socket.\n");
    return -1;
  }
  return 0;
}

int xl3_rw(uint32_t address, uint32_t data, uint32_t *result, int crate_num)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = FAST_CMD_ID;
  FECCommand *command;
  command = (FECCommand *) packet.payload;
  command->flags = 0x0;
  command->cmd_num = 0x0;
  command->packet_num = 0x0;
  command->address = address;
  command->data = data;
  SwapLongBlock(&(command->data),1);
  SwapLongBlock(&(command->address),1);
  do_xl3_cmd(&packet, crate_num);
  *result = command->data;
  SwapLongBlock(result,1);
  return command->flags;
}

int wait_for_multifc_results(int num_cmds, int packet_num, int xl3num, uint32_t *buffer)
{
  if (rw_xl3_fd[xl3num] <= 0){
    pt_printsend("wait_for_multifc_results: Invalid crate number (%d): socket value is NULL\n",xl3num);
    return -1;
  }

  int i,current_num = 0;
  // First we check if there's any acks left in our buffer
  if (multifc_buffer_full[xl3num] != 0){
    for (i=0;i<multifc_buffer[xl3num].howmany;i++){
      // loop through each command ack in the buffer and see if any
      // matches the command number that we are waiting for
      if (multifc_buffer[xl3num].cmd[i].packet_num == packet_num){
        // if it is we make sure that we are getting the individual
        // rw acks in order
        if (multifc_buffer[xl3num].cmd[i].cmd_num == current_num){
          // if they are in the right order, add them to the result array
          *(buffer + current_num) = multifc_buffer[xl3num].cmd[i].data;
          if (multifc_buffer[xl3num].cmd[i].flags != 0){
            pt_printsend("wait_for_multifc_results: There was a bus error in results\n");
          }
          current_num++;
        }else{
          pt_printsend("wait_for_multifc_results: Results out of order?\n");
          return -2;
        }
        if (i == (multifc_buffer[xl3num].howmany-1)){
          multifc_buffer_full[xl3num] = 0;
          multifc_buffer[xl3num].howmany = 0;
        }
      }
    }
  }

  XL3_Packet packet;
  fd_set readable_fdset;
  FD_ZERO(&readable_fdset);
  FD_SET(rw_xl3_fd[xl3num],&readable_fdset);

  struct timeval delay_value;
  delay_value.tv_sec = 4;
  delay_value.tv_usec = 0;
  // lets read more from the xl3
  while (1){
    memset(&packet,'\0',MAX_PACKET_SIZE);
    int data = select(fdmax+1,&readable_fdset,NULL,NULL,&delay_value);
    // check for errors
    if (data == -1){
      pt_printsend("wait_for_multifc_results: Error in select\n");
      return -2;
    }else if (data == 0){
      pt_printsend("wait_for_multifc_results: Timeout in select\n");
      return -3;
    }
    // lets see whats readable
    if (FD_ISSET(rw_xl3_fd[xl3num],&readable_fdset)){
      int n = recv(rw_xl3_fd[xl3num],(char *)&packet,MAX_PACKET_SIZE,0); 
      if (n < 0){
        pt_printsend("wait_for_multifc_results: Error receiving data from XL3 #%d. Closing connection\n",xl3num);
        pthread_mutex_lock(&main_fdset_lock);
        FD_CLR(rw_xl3_fd[xl3num],&xl3_fdset);
        FD_CLR(rw_xl3_fd[xl3num],&main_fdset);
        close(rw_xl3_fd[xl3num]);
        xl3_connected[xl3num] = 0;
        rw_xl3_fd[xl3num] = -1;
        pthread_mutex_unlock(&main_fdset_lock);
        return -1;
      }else if (n == 0){
        pt_printsend("wait_for_multifc_results: Got a zero byte packet, Closing XL3 #%d\n",xl3num);
        pthread_mutex_lock(&main_fdset_lock);
        FD_CLR(rw_xl3_fd[xl3num],&xl3_fdset);
        FD_CLR(rw_xl3_fd[xl3num],&main_fdset);
        close(rw_xl3_fd[xl3num]);
        xl3_connected[xl3num] = 0;
        rw_xl3_fd[xl3num] = -1;
        pthread_mutex_unlock(&main_fdset_lock);
        return -1;
      }

      SwapShortBlock(&(packet.cmdHeader.packet_num),1);
      // if its a message we'll print it out and loop around again
      if (packet.cmdHeader.packet_type == MESSAGE_ID){
        pt_printsend("%s",packet.payload);
      }else if (packet.cmdHeader.packet_type == PING_ID){
        packet.cmdHeader.packet_type = PONG_ID;
        n = write(rw_xl3_fd[xl3num],(char *)&packet,MAX_PACKET_SIZE);
        if (n < 0){
          pt_printsend("wait_for_multifc_results: Error writing to socket for pong.\n");
          return -1;
        }
      }else if (packet.cmdHeader.packet_type == MEGA_BUNDLE_ID){
        //store_mega_bundle();
      }else if (packet.cmdHeader.packet_type == CMD_ACK_ID){
        // read in cmds and add to buffer, then loop again unless read in num_cmds cmds
        MultiFC commands;
        commands = *(MultiFC *) packet.payload;
        SwapLongBlock(&(commands.howmany),1);
        for (i=0;i<MAX_ACKS_SIZE;i++){
          SwapLongBlock(&(commands.cmd[i].cmd_num),1);
          SwapShortBlock(&(commands.cmd[i].packet_num),1);
          SwapLongBlock(&(commands.cmd[i].data),1);
        }
        for (i=0;i<commands.howmany;i++){
          // check if it has the right packet number
          if (commands.cmd[i].packet_num == packet_num){
            // if it does, make sure the individual rw acks are coming in order
            if (commands.cmd[i].cmd_num == current_num){
              // if they are, add to the buffer
              *(buffer + current_num) = commands.cmd[i].data;
              if (commands.cmd[i].flags != 0)
                printsend("wait_for_multifc_results: Bus error, %02x, command # %d\n",commands.cmd[i].flags,i);
              current_num++;
            }else{
              pt_printsend("wait_for_multifc_results: Results out of order?\n");
              return -1;
            }
          }else{
            // we have results from a different packet, so buffer it
            //  and let the next call to this function get it
            if (multifc_buffer_full[xl3num] == 0){
              multifc_buffer_full[xl3num] = 1;
              multifc_buffer[xl3num].cmd[multifc_buffer[xl3num].howmany] = commands.cmd[i];
              multifc_buffer[xl3num].howmany++;
            }else{
              pt_printsend("wait_for_multifc_results: Result buffer already full, packets mixed up?\n");
              return -1;
            }
          }
        } // end looping over all commands in this packet
        if (current_num == num_cmds){
          return 0; // we got all our results, otherwise go around again
        }
      }else{
        // got the wrong type of ack packet
        pt_printsend("wait_for_multifc_results: Got wrong packet type back, got %08x\n",packet.cmdHeader.packet_type);
      } // end switch on packet type
    }else{
      // ?? its the only thing we're looking at
      pt_printsend("wait_for_multifc_result: Somehow xl3 still not readable\n");
      return -4;
    } // end if xl3 is readable
  } // end while loop

  // shouldnt ever get here
  return -1;
}

