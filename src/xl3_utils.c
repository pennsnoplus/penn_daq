#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xl3_types.h"

#include "net_utils.h"
#include "daq_utils.h"
#include "xl3_utils.h"

int deselect_fecs(int crate)
{
  return 0;
}

int update_crate_config(int crate, uint16_t slot_mask)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = BUILD_CRATE_CONFIG_ID;
  *(uint32_t *) packet.payload = slot_mask;
  SwapLongBlock(packet.payload,1);
  do_xl3_cmd(&packet,crate);

  int errors = *(uint32_t *) packet.payload;
  int j;
  for (j=0;j<16;j++){
    if ((0x1<<j) & slot_mask){
      crate_config[crate][j] = *(hware_vals_t *) (packet.payload+4+j*sizeof(hware_vals_t));
      SwapShortBlock(&(crate_config[crate][j].mb_id),1);
      SwapShortBlock((crate_config[crate][j].dc_id),4);
    }
  }
  deselect_fecs(crate);
  return 0;
}

int debugging_mode(char *buffer, int onoff)
{
  int crate_num = 2;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          crate_num = atoi(words2);
      }else if (words[1] == 'h'){
        printsend("Usage: debugging_on/off -c [crate num (int)]\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }
  
  // now check and see if everything needed is unlocked
  if (xl3_lock[crate_num] == 0){
    // spawn a thread to do it
    xl3_lock[crate_num] = 1;

    pthread_t *new_thread;
    new_thread = malloc(sizeof(pthread_t));
    int i,thread_num = -1;
    for (i=0;i<MAX_THREADS;i++){
      if (thread_pool[i] == NULL){
        thread_pool[i] = new_thread;
        thread_num = i;
        break;
      }
    }
    if (thread_num == -1){
      printsend("All threads busy currently\n");
      return -1;
    }
    uint32_t *args;
    args = malloc(3*sizeof(uint32_t));
    args[0] = (uint32_t) thread_num;
    args[1] = (uint32_t) crate_num;
    args[2] = (uint32_t) onoff;
    pthread_create(new_thread,NULL,pt_debugging_mode,(void *)args);
  }else{
    // this xl3 is locked, we cant do this right now
    return -1;
  }
  return 0; 
}

void *pt_debugging_mode(void* args)
{
  int thread_num = ((uint32_t *) args)[0];
  int crate_num = ((uint32_t *) args)[1];
  uint32_t onoff = ((uint32_t *) args)[2];
  free(args);

  fd_set threadset;
  FD_ZERO(&threadset);
  FD_SET(rw_xl3_fd[crate_num],&threadset);

  XL3_Packet debug_packet;
  memset(&debug_packet,0,sizeof(XL3_Packet));
  debug_packet.cmdHeader.packet_type = DEBUGGING_MODE_ID;
  *(uint32_t *) debug_packet.payload = (uint32_t) onoff;
  SwapLongBlock(debug_packet.payload,1);

  int result = do_xl3_cmd(&debug_packet,crate_num,&threadset);

  if (result == 0){ 
    if (onoff == 1)
      pt_printsend("Debugging turned on\n");
    else
      pt_printsend("Debugging turned off\n");
  }

  xl3_lock[crate_num] = 0;
  thread_done[thread_num] = 1;
}

int change_mode(char *buffer)
{
  int crate_num = 2;
  int mode = 1;
  uint16_t davail_mask = 0x0000;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          crate_num = atoi(words2);
      }else if (words[1] == 'n'){
        mode = 2;
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          davail_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        printsend("Usage: change_mode -c [crate num (int)]"
          "-n (normal mode) -i (init mode) -s [data available mask (hex)]\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (xl3_lock[crate_num] == 0){
    // spawn a thread to do it
    xl3_lock[crate_num] = 1;

    pthread_t *new_thread;
    new_thread = malloc(sizeof(pthread_t));
    int i,thread_num = -1;
    for (i=0;i<MAX_THREADS;i++){
      if (thread_pool[i] == NULL){
        thread_pool[i] = new_thread;
        thread_num = i;
        break;
      }
    }
    if (thread_num == -1){
      printsend("All threads busy currently\n");
      return -1;
    }
    uint32_t *args;
    args = malloc(3*sizeof(uint32_t));
    args[0] = (uint32_t) thread_num;
    args[1] = (uint32_t) crate_num;
    args[2] = (uint32_t) mode;
    args[3] = (uint32_t) davail_mask;
    pthread_create(new_thread,NULL,pt_change_mode,(void *)args);
  }else{
    // this xl3 is locked, we cant do this right now
    return -1;
  }
  return 0; 
}

void *pt_change_mode(void* args)
{
  int thread_num = ((uint32_t *) args)[0];
  int crate_num = ((uint32_t *) args)[1];
  int mode = ((uint32_t *) args)[2];
  uint16_t davail_mask = ((uint32_t *) args)[3];
  free(args);

  fd_set threadset;
  FD_ZERO(&threadset);
  FD_SET(rw_xl3_fd[crate_num],&threadset);

  XL3_Packet packet;
  memset(&packet,0,sizeof(XL3_Packet));
  packet.cmdHeader.packet_type = CHANGE_MODE_ID;
  *(uint32_t *) packet.payload = (uint32_t) mode;
  *(((uint32_t *) packet.payload)+1) = (uint32_t) davail_mask;
  SwapLongBlock(packet.payload,1);

  int result = do_xl3_cmd(&packet,crate_num,&threadset);

  if (result == 0){ 
    if (mode == 1)
      pt_printsend("Mode changed to init mode\n");
    else
      pt_printsend("Mode changed to normal mode\n");
  }

  xl3_lock[crate_num] = 0;
  thread_done[thread_num] = 1;
}
