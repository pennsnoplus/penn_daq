#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "xl3_registers.h"

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "xl3_utils.h"
#include "xl3_cmds.h"

int sm_reset(char *buffer)
{
  sm_reset_t *args;
  args = malloc(sizeof(sm_reset_t));

  args->crate_num = 2;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'h'){
        pt_printsend( "Usage: sm_reset -c [crate num (int)]\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (xl3_lock[args->crate_num] != 0){
    // this xl3 is locked, we cant do this right now
    free(args);
    return -1;
  }
  if (xl3_connected[args->crate_num] == 0){
    printsend("XL3 #%d is not connected! Aborting\n",args->crate_num);
    free(args);
    return 0;
  }
  // spawn a thread to do it
  xl3_lock[args->crate_num] = 1;

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
    free(args);
    return -1;
  }
  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_sm_reset,(void *)args);
  return 0; 
}

void *pt_sm_reset(void *args)
{
  sm_reset_t arg = *(sm_reset_t *) args;
  free(args);

  XL3_Packet packet;
  packet.cmdHeader.packet_type = STATE_MACHINE_RESET_ID;
  do_xl3_cmd(&packet,arg.crate_num);

  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int cmd_xl3_rw(char *buffer)
{
  cmd_xl3_rw_t *args;
  args = malloc(sizeof(cmd_xl3_rw_t));

  args->crate_num = 2;
  args->address = 0x02000007;
  args->data = 0x00000000;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'a'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->address = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->data = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: xl3_rw -c [crate_num (int)] "
            "-a [address (hex)] -d [data (hex)]\n");
        pt_printsend("Please check xl3/xl3_registers.h for the address mapping\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (xl3_lock[args->crate_num] != 0){
    // this xl3 is locked, we cant do this right now
    free(args);
    return -1;
  }
  if (xl3_connected[args->crate_num] == 0){
    printsend("XL3 #%d is not connected! Aborting\n",args->crate_num);
    free(args);
    return 0;
  }
  // spawn a thread to do it
  xl3_lock[args->crate_num] = 1;

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
    free(args);
    return -1;
  }
  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_xl3_rw,(void *)args);
  return 0; 
}

void *pt_cmd_xl3_rw(void *args)
{
  cmd_xl3_rw_t arg = *(cmd_xl3_rw_t *) args;
  free(args);

  uint32_t result = 0x0;
  int errors = xl3_rw(arg.address, arg.data, &result, arg.crate_num);
  if (errors == 0)
    pt_printsend( "result was %08x\n",result);
  else 
    pt_printsend("there was a bus error!\n");

  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int xl3_queue_rw(char *buffer)
{
  cmd_xl3_rw_t *args;
  args = malloc(sizeof(cmd_xl3_rw_t));

  args->crate_num = 2;
  args->address = 0x02000007;
  args->data = 0x00000000;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'a'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->address = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->data = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: xl3_queue_rw -c [crate_num (int)] "
            "-a [address (hex)] -d [data (hex)]\n");
        pt_printsend("Please check xl3/xl3_registers.h for the address mapping\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (xl3_lock[args->crate_num] != 0){
    // this xl3 is locked, we cant do this right now
    free(args);
    return -1;
  }
  if (xl3_connected[args->crate_num] == 0){
    printsend("XL3 #%d is not connected! Aborting\n",args->crate_num);
    free(args);
    return 0;
  }
  // spawn a thread to do it
  xl3_lock[args->crate_num] = 1;

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
    free(args);
    return -1;
  }
  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_xl3_queue_rw,(void *)args);
  return 0; 
}

void *pt_xl3_queue_rw(void *args)
{
  cmd_xl3_rw_t arg = *(cmd_xl3_rw_t *) args;
  free(args);

  XL3_Packet packet;
  MultiFC *commands = (MultiFC *) packet.payload;
  commands->howmany = 1;
  commands->cmd[0].address = arg.address;
  commands->cmd[0].data = arg.data;
  SwapLongBlock(&(commands->cmd[0].address),1);
  SwapLongBlock(&(commands->cmd[0].data),1);
  SwapLongBlock(&(commands->howmany),1);

  packet.cmdHeader.packet_type = QUEUE_CMDS_ID;
  int errors = do_xl3_cmd(&packet,arg.crate_num);
  if (errors < 0){
    pt_printsend("Error queuing command\n");
    xl3_lock[arg.crate_num] = 0;
    thread_done[arg.thread_num] = 1;
    return;
  }
  
  uint32_t result;
  errors = wait_for_multifc_results(1,command_number[arg.crate_num]-1,arg.crate_num,&result);
  if (errors < 0){
    pt_printsend("Error getting result\n");
    xl3_lock[arg.crate_num] = 0;
    thread_done[arg.thread_num] = 1;
    return;
  }

  pt_printsend("result was %08x\n",result);
  
  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return;
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
  if (xl3_lock[crate_num] != 0){
    // this xl3 is locked, we cant do this right now
    return -1;
  }
  if (xl3_connected[crate_num] == 0){
    printsend("XL3 #%d is not connected! Aborting\n",crate_num);
    return 0;
  }
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
  if (xl3_lock[crate_num] != 0){
    // this xl3 is locked, we cant do this right now
    return -1;
  }
  if (xl3_connected[crate_num] == 0){
    printsend("XL3 #%d is not connected! Aborting\n",crate_num);
    return 0;
  }

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
