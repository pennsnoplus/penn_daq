#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#include "packet_types.h"

#include "daq_utils.h"
#include "net.h"
#include "net_utils.h"

int do_xl3_cmd(XL3_Packet *packet,int xl3num, fd_set *thread_fdset)
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

  fd_set readable_fdset = *(thread_fdset);

  struct timeval delay_value;
  if (sent_packet_type == MEM_TEST_ID || sent_packet_type == ZDISC_ID)
    delay_value.tv_sec = 60;
  else
    delay_value.tv_sec = 10;
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
    int i;
    for (i=0;i<MAX_XL3_CON;i++){
      if (rw_xl3_fd[i] > 0){
        if (FD_ISSET(rw_xl3_fd[i],&readable_fdset)){
          n = recv(rw_xl3_fd[i],(char *)packet,MAX_PACKET_SIZE,0); 
          if (n < 0){
            pt_printsend("do_xl3_cmd: Error receiving data from XL3 #%d. Closing connection\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }else if (n == 0){
            pt_printsend("Got a zero byte packet, Closing XL3 #%d\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }

          SwapShortBlock(&(packet->cmdHeader.packet_num),1);
          if (packet->cmdHeader.packet_type == MESSAGE_ID){
            pt_printsend("%s",packet->payload);
          }else if (packet->cmdHeader.packet_type == MEGA_BUNDLE_ID){
            //store_mega_bundle();
          }else if (packet->cmdHeader.packet_type == SCREWED_ID){
            //handle_screwed_packet();
          }else if (packet->cmdHeader.packet_type == ERROR_ID){
            //handle_error_packet();
          }else if (packet->cmdHeader.packet_type == PING_ID){
            packet->cmdHeader.packet_type = PONG_ID;
            n = write(rw_xl3_fd[i],(char *)packet,MAX_PACKET_SIZE);
            if (n < 0){
              pt_printsend("do_xl3_cmd: Error writing to socket for pong.\n");
              return -1;
            }
          }else if (i == xl3num){
            if (packet->cmdHeader.packet_type == sent_packet_type){
              // its the right packet type, make sure number is correct
              if (packet->cmdHeader.packet_num == (sent_command_number)%65536){
                return 0;
              }else{
                pt_printsend("do_xl3_cmd(): Got wrong command number back. Expected %d, got %d.\n",sent_command_number,packet->cmdHeader.packet_num);
              }
            }else{
              // got the wrong type of ack packet
              pt_printsend("Got wrong packet type back do_xl3_cmd(): Expected %08x, got %08x (packet num %d)\n",sent_packet_type,packet->cmdHeader.packet_type,packet->cmdHeader.packet_num);
              int q;
              uint32_t *point = (uint32_t *) packet->payload;
              for (q=0;q<10;q++){
                pt_printsend("%d %08x\n",q,*(point));
                point++;
              }
            } // end if this was xl3num
          }else{
            pt_printsend("do_xl3_cmd: Wasn't expecting anything from xl3 %d, got a %d type packet. Was waiting for a %d type packet from xl3 %d.\n",i,packet->cmdHeader.packet_type,sent_packet_type,xl3num);
          } // end switch on packet type
        } // end if this xl3 is readable 
      } // end if this xl3 is connected
    } // end loop on xl3 fds
  } // end while loop

  // should never get here
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

int xl3_rw(uint32_t address, uint32_t data, uint32_t *result, int crate_num, fd_set *thread_fdset)
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
  do_xl3_cmd(&packet, crate_num, thread_fdset);
  *result = command->data;
  SwapLongBlock(result,1);
  SwapShortBlock(&command->flags,1);
  return (int) command->flags;
}

int wait_for_multifc_results(int num_cmds, int packet_num, int xl3num, uint32_t *buffer, fd_set *thread_fdset)
{
  if (rw_xl3_fd[xl3num] <= 0){
    pt_printsend("wait_for_multifc_results: Invalid crate number (%d): socket value is NULL\n",xl3num);
    return -1;
  }

  int i,current_num = 0;
  // First we check if there's any acks left in our buffer
  if (multifc_buffer_full[xl3num] != 0){
    pt_printsend("there was stuff in the buffer\n");
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
      }else{
        pt_printsend("There's already stuff waiting and its not for us?\n");
      }
    }
  }

  XL3_Packet packet;
  fd_set readable_fdset = *(thread_fdset);

  struct timeval delay_value;
  delay_value.tv_sec = 30;
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
    for (i=0;i<MAX_XL3_CON;i++){
      if (rw_xl3_fd[i] > 0){
        if (FD_ISSET(rw_xl3_fd[i],&readable_fdset)){
          int n = recv(rw_xl3_fd[i],(char *)&packet,MAX_PACKET_SIZE,0); 
          if (n < 0){
            pt_printsend("wait_for_multifc_results: Error receiving data from XL3 #%d. Closing connection\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }else if (n == 0){
            pt_printsend("wait_for_multifc_results: Got a zero byte packet, Closing XL3 #%d\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }

          SwapShortBlock(&(packet.cmdHeader.packet_num),1);
          if (packet.cmdHeader.packet_type == MESSAGE_ID){
            pt_printsend("%s",packet.payload);
          }else if (packet.cmdHeader.packet_type == MEGA_BUNDLE_ID){
            //store_mega_bundle();
          }else if (packet.cmdHeader.packet_type == SCREWED_ID){
            //handle_screwed_packet();
          }else if (packet.cmdHeader.packet_type == ERROR_ID){
            //handle_error_packet();
          }else if (packet.cmdHeader.packet_type == PING_ID){
            packet.cmdHeader.packet_type = PONG_ID;
            n = write(rw_xl3_fd[i],(char *)&packet,MAX_PACKET_SIZE);
            if (n < 0){
              pt_printsend("wait_for_multifc_results: Error writing to socket for pong.\n");
              return -1;
            }
          }else if (i == xl3num){
            if (packet.cmdHeader.packet_type == CMD_ACK_ID){
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
                if (commands.cmd[i].packet_num == (packet_num%65536)){
                  // if it does, make sure the individual rw acks are coming in order
                  if (commands.cmd[i].cmd_num == (current_num)){
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
                  pt_printsend("expecting packet number %d, got number %d\n",packet_num,commands.cmd[i].packet_num);
                  // we have results from a different packet, so buffer it
                  //  and let the next call to this function get it
                  //if (multifc_buffer_full[xl3num] == 0){
                    multifc_buffer_full[xl3num] = 1;
                    multifc_buffer[xl3num].cmd[multifc_buffer[xl3num].howmany] = commands.cmd[i];
                    multifc_buffer[xl3num].howmany++;
                  //}else{
                  //  pt_printsend("wait_for_multifc_results: Result buffer already full, packets mixed up?\n");
                  //  return -1;
                  //}
                }
              } // end looping over all commands in this packet
              if (current_num == num_cmds){
                return 0; // we got all our results, otherwise go around again
              }
            }else{
              // got the wrong type of ack packet
              pt_printsend("wait_for_multifc_results: Got wrong packet type back, Expected %08x, got %08x (packet num %d)\n",CMD_ACK_ID,packet.cmdHeader.packet_type,packet.cmdHeader.packet_num);
              int q;
              uint32_t *point = (uint32_t *) packet.payload;
              for (q=0;q<10;q++){
                pt_printsend("%d %08x\n",q,*(point));
                point++;
              }
            } // end if i = xl3num
          }else{
            pt_printsend("wait_for_multifc_results: Wasn't expecting a packet from xl3 %d, got type %d\n",i,packet.cmdHeader.packet_type);
          } // end switch on packet type
        } // end if this xl3 is readable
      } // end if this xl3 is connected
    } // end loop over xl3s
  } // end while loop

  // shouldnt ever get here
  return -1;
}

int wait_for_cald_test_results(int xl3num, uint16_t *point_buf, uint16_t *adc_buf, fd_set *thread_fdset)
{
  if (rw_xl3_fd[xl3num] <= 0){
    pt_printsend("wait_for_cald_test_results: Invalid crate number (%d): socket value is NULL\n",xl3num);
    return -1;
  }

  int i,j;
  int point_count = 0;
  int current_slot = 0;
  int current_point = 0;

  XL3_Packet packet;
  fd_set readable_fdset = *(thread_fdset);

  struct timeval delay_value;
  delay_value.tv_sec = 10;
  delay_value.tv_usec = 0;
  // we loop, taking packets from our XL3s. We are expecting
  // cald response packets which we parse and add to our buffers.
  // when we get a cald test id packet, it means we are done
  while (1){
    memset(&packet,'\0',MAX_PACKET_SIZE);
    int data = select(fdmax+1,&readable_fdset,NULL,NULL,&delay_value);
    // check for errors
    if (data == -1){
      pt_printsend("wait_for_cald_test_results: Error in select\n");
      return -2;
    }else if (data == 0){
      pt_printsend("wait_for_cald_test_results: Timeout in select\n");
      return -3;
    }
    // lets see whats readable
    for (i=0;i<MAX_XL3_CON;i++){
      if (rw_xl3_fd[i] > 0){
        if (FD_ISSET(rw_xl3_fd[i],&readable_fdset)){
          int n = recv(rw_xl3_fd[i],(char *)&packet,MAX_PACKET_SIZE,0); 
          if (n < 0){
            pt_printsend("wait_for_cald_test_results: Error receiving data from XL3 #%d. Closing connection\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }else if (n == 0){
            pt_printsend("wait_for_cald_test_results: Got a zero byte packet, Closing XL3 #%d\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }

          SwapShortBlock(&(packet.cmdHeader.packet_num),1);
          if (packet.cmdHeader.packet_type == MESSAGE_ID){
            pt_printsend("%s",packet.payload);
          }else if (packet.cmdHeader.packet_type == MEGA_BUNDLE_ID){
            //store_mega_bundle();
          }else if (packet.cmdHeader.packet_type == SCREWED_ID){
            //handle_screwed_packet();
          }else if (packet.cmdHeader.packet_type == ERROR_ID){
            //handle_error_packet();
          }else if (packet.cmdHeader.packet_type == PING_ID){
            packet.cmdHeader.packet_type = PONG_ID;
            n = write(rw_xl3_fd[i],(char *)&packet,MAX_PACKET_SIZE);
            if (n < 0){
              pt_printsend("wait_for_cald_test_results: Error writing to socket for pong.\n");
              return -1;
            }
          }else if (i == xl3num){
            if (packet.cmdHeader.packet_type == CALD_RESPONSE_ID){
              cald_response_results_t *packet_results = (cald_response_results_t *) packet.payload;
              SwapShortBlock(packet_results,sizeof(cald_response_results_t)/sizeof(uint16_t));
              if (packet_results->slot != current_slot){
                current_slot = packet_results->slot;
                current_point = 0;
              }
              for (j=0;j<100;j++){
                if (packet_results->point[j] != 0){
                  point_buf[current_slot*10000+current_point] = packet_results->point[j];
                  point_buf[0] = packet_results->point[j];
                  adc_buf[current_slot*4*10000+current_point*4+0] = packet_results->adc0[j];
                  adc_buf[current_slot*4*10000+current_point*4+1] = packet_results->adc1[j];
                  adc_buf[current_slot*4*10000+current_point*4+2] = packet_results->adc2[j];
                  adc_buf[current_slot*4*10000+current_point*4+3] = packet_results->adc3[j];
                  current_point++;
                  point_count++;
                }
              }
            }else if (packet.cmdHeader.packet_type == CALD_TEST_ID){
              // we must be finished
              return point_count;
            }else{
              // got the wrong type of ack packet
              pt_printsend("wait_for_cald_test_results: Got wrong packet type back, got %08x\n",packet.cmdHeader.packet_type);
            } // end if i = xl3num
          }else{
            pt_printsend("wait_for_cald_test_results: Wasn't expecting a packet from xl3 %d, got type %d\n",i,packet.cmdHeader.packet_type);
          } // end switch on packet type
        } // end if this xl3 is readable
      } // end if this xl3 is connected
    } // end loop over xl3s
  } // end while loop

  // shouldnt ever get here
  return -1;
}

/*
int read_from_tut(char *result, fd_set *thread_fdset)
{
  if (rw_cont_fd <= 0){
    pt_printsend("read_from_tut: Controller doesn't appear to be connected");
    return -1;
  }

  fd_set readable_fdset = *(thread_fdset);
  fd_set writeable_fdset = *(thread_fdset);
  int j;
  for (j=0;j<=fdmax;j++)
    if (FD_ISSET(j,&readable_fdset))
      pt_printsend("%d is set\n",j);

  XL3_Packet packet;
  char tut_buf[2000];
  memset(tut_buf,'\0',2000);

  struct timeval delay_value;
  delay_value.tv_sec = 10;
  delay_value.tv_usec = 0;
  // lets get a response back from the xl3
  // or some text from the controller
  while (1){
    memset(&packet,'\0',MAX_PACKET_SIZE);
    int data = select(fdmax+1,&readable_fdset,&writeable_fdset,NULL,&delay_value);
    // check for errors
    if (data == -1){
      pt_printsend("read_from_tut: Error in select\n");
      return -2;
    }else if (data == 0){
      pt_printsend("read_from_tut: Timeout in select\n");
      return -3;
    }
    // lets see whats readable
    int i,n;
    for (i=0;i<=fdmax;i++){
      if (FD_ISSET(i,&readable_fdset)){
        pt_printsend("%d is readabe\n",i);
        if (FD_ISSET(i,&cont_fdset))
          pt_printsend("Its a controller\n");
      }
    }
    for (i=0;i<MAX_XL3_CON;i++){
      if (rw_xl3_fd[i] > 0){
        if (FD_ISSET(rw_xl3_fd[i],&readable_fdset)){
          n = recv(rw_xl3_fd[i],(char*)&packet,MAX_PACKET_SIZE,0); 
          if (n < 0){
            pt_printsend("read_from_tut: Error receiving data from XL3 #%d. Closing connection\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }else if (n == 0){
            pt_printsend("read_from_tut: Got a zero byte packet, Closing XL3 #%d\n",i);
            pthread_mutex_lock(&main_fdset_lock);
            FD_CLR(rw_xl3_fd[i],&xl3_fdset);
            FD_CLR(rw_xl3_fd[i],&main_fdset);
            close(rw_xl3_fd[i]);
            rw_xl3_fd[i] = -1;
            pthread_mutex_unlock(&main_fdset_lock);
            return -1;
          }

          SwapShortBlock(&(packet.cmdHeader.packet_num),1);
          if (packet.cmdHeader.packet_type == MESSAGE_ID){
            pt_printsend("%s",packet.payload);
          }else if (packet.cmdHeader.packet_type == MEGA_BUNDLE_ID){
            //store_mega_bundle();
          }else if (packet.cmdHeader.packet_type == SCREWED_ID){
            //handle_screwed_packet();
          }else if (packet.cmdHeader.packet_type == ERROR_ID){
            //handle_error_packet();
          }else if (packet.cmdHeader.packet_type == PING_ID){
            packet.cmdHeader.packet_type = PONG_ID;
            n = write(rw_xl3_fd[i],(char *)&packet,MAX_PACKET_SIZE);
            if (n < 0){
              pt_printsend("do_xl3_cmd: Error writing to socket for pong.\n");
              return -1;
            }
          } // end switch on packet type
        } // end if this xl3 is readable 
      } // end if this xl3 is connected
    } // end loop on xl3 fds
    // now check controller for text
    if (FD_ISSET(rw_cont_fd,&readable_fdset)){
      pt_printsend("got a cont packet\n");
      n = recv(rw_cont_fd,tut_buf,2000,0); 
      pt_printsend("size was %d\n",n);
      if (n > 0){
        sprintf(result,"%s",tut_buf);
        if (FD_ISSET(rw_cont_fd,&writeable_fdset)){
          write(rw_cont_fd,CONT_CMD_ACK,strlen(CONT_CMD_ACK));
          return 0;
        }else{
          pt_printsend("Could not ack to controller. Check connection\n");
          return 0;
        }
      }else if (n < 0){
        pt_printsend("Error receiving command from controller\n");
        pthread_mutex_lock(&main_fdset_lock);
        FD_CLR(rw_cont_fd,&cont_fdset);
        FD_CLR(rw_cont_fd,&main_fdset);
        close(rw_cont_fd);
        pthread_mutex_unlock(&main_fdset_lock);
        rw_cont_fd = -1;
        return -1;
      }else if (n == 0){
        pt_printsend("Closing controller connection.\n");
        pthread_mutex_lock(&main_fdset_lock);
        FD_CLR(rw_cont_fd,&cont_fdset);
        FD_CLR(rw_cont_fd,&main_fdset);
        close(rw_cont_fd);
        pthread_mutex_unlock(&main_fdset_lock);
        rw_cont_fd = -1;
        return -1;
      }
    } // end if packet from controller
  } // end while loop

  // should never get here
  return 0;
}
*/
