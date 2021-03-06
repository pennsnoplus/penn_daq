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

int xrw(char *buffer, int rw)
{
  xrw_t *args;
  args = malloc(sizeof(xrw_t));

  args->rw = rw;
  args->crate_num = 2;
  args->register_num = 0;
  args->data = 0x0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'r'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->register_num = atoi(words2);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->data = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        if (rw == 0)
          pt_printsend("Usage: xr -c [crate num (int)] -r [register number (int)]\n");
        else if (rw == 1)
          pt_printsend("Usage: xw -c [crate num (int)] -r [register number (int)] -d [data (hex)]\n");
        pt_printsend("type \"help xl3_registers\" to get "
            "a list of registers with numbers and descriptions\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_xrw,(void *)args);
  return 0; 
}

void *pt_xrw(void *args)
{
  xrw_t arg = *(xrw_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  if (arg.register_num > 13){
    pt_printsend("Not a valid register.\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  uint32_t result;
  if (arg.rw == 0){
    int errors = xl3_rw(xl3_reg_addresses[arg.register_num] + READ_REG,0x0, &result, arg.crate_num,&thread_fdset);
    pt_printsend("%08x\n",result);
    //pt_printsend("Read out %08x from register %d (%08x)\n",result,arg.register_num,xl3_reg_addresses[arg.register_num]);
  }else{
    int errors = xl3_rw(xl3_reg_addresses[arg.register_num] + WRITE_REG,arg.data, &result, arg.crate_num,&thread_fdset);
    pt_printsend("%08x\n",result);
    //pt_printsend("Wrote %08x to register %d (%08x)\n",arg.data,arg.register_num,xl3_reg_addresses[arg.register_num]);
  }

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
  return;
}

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

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
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

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  XL3_Packet packet;
  packet.cmdHeader.packet_type = STATE_MACHINE_RESET_ID;
  do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
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

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
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

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  uint32_t result = 0x0;
  int errors = xl3_rw(arg.address, arg.data, &result, arg.crate_num,&thread_fdset);
  if (errors == 0)
    pt_printsend( "result was %08x\n",result);
  else 
    pt_printsend("there was a bus error! %d\n",errors);

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
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

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
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

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  XL3_Packet packet;
  MultiFC *commands = (MultiFC *) packet.payload;
  commands->howmany = 1;
  commands->cmd[0].address = arg.address;
  commands->cmd[0].data = arg.data;
  SwapLongBlock(&(commands->cmd[0].address),1);
  SwapLongBlock(&(commands->cmd[0].data),1);
  SwapLongBlock(&(commands->howmany),1);

  packet.cmdHeader.packet_type = QUEUE_CMDS_ID;
  int errors = do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
  if (errors < 0){
    pt_printsend("Error queuing command\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }
  
  uint32_t result;
  errors = wait_for_multifc_results(1,command_number[arg.crate_num]-1,arg.crate_num,&result,&thread_fdset);
  if (errors < 0){
    pt_printsend("Error getting result\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  pt_printsend("result was %08x\n",result);
  
  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
  return;
}

int debugging_mode(char *buffer, int onoff)
{
  debugging_mode_t *args;
  args = malloc(sizeof(debugging_mode_t));

  args->crate_num = 2;
  args->onoff = onoff;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'h'){
        printsend("Usage: debugging_on/off -c [crate num (int)]\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_debugging_mode,(void *)args);
  return 0; 
}

void *pt_debugging_mode(void* args)
{
  int thread_num = ((uint32_t *) args)[0];
  int crate_num = ((uint32_t *) args)[1];
  uint32_t onoff = ((uint32_t *) args)[2];
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[crate_num],&thread_fdset);

  XL3_Packet debug_packet;
  memset(&debug_packet,0,sizeof(XL3_Packet));
  debug_packet.cmdHeader.packet_type = DEBUGGING_MODE_ID;
  *(uint32_t *) debug_packet.payload = (uint32_t) onoff;
  SwapLongBlock(debug_packet.payload,1);

  int result = do_xl3_cmd(&debug_packet,crate_num,&thread_fdset);

  if (result == 0){ 
    if (onoff == 1)
      pt_printsend("Debugging turned on\n");
    else
      pt_printsend("Debugging turned off\n");
  }

  unthread_and_unlock(0,(0x1<<crate_num),thread_num);
}

int cmd_change_mode(char *buffer)
{
  cmd_change_mode_t *args;
  args = malloc(sizeof(cmd_change_mode_t));
  
  args->crate_num = 2;
  args->mode = INIT_MODE;
  args->davail_mask = 0x0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'n'){
        args->mode = NORMAL_MODE;
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->davail_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        printsend("Usage: change_mode -c [crate num (int)]"
            "-n (normal mode) -i (init mode) -s [data available mask (hex)]\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_change_mode,(void *)args);
  return 0; 
}

void *pt_cmd_change_mode(void* args)
{
  cmd_change_mode_t arg = *(cmd_change_mode_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int result = change_mode(arg.mode,arg.davail_mask,arg.crate_num,&thread_fdset);

  if (result == 0){ 
    if (arg.mode == 1)
      pt_printsend("Mode changed to init mode\n");
    else
      pt_printsend("Mode changed to normal mode\n");
  }

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

int read_local_voltage(char *buffer)
{
  read_local_voltage_t *args;
  args = malloc(sizeof(read_local_voltage_t));

  args->crate_num = 2;
  args->voltage_select = 0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'v'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->voltage_select = atoi(words2);
      }else if (words[1] == 'h'){
        printsend("Usage: read_local_voltage -c [crate num (int)]"
            "-v [voltage number]\n");
        printsend("0 - VCC\n1 - VEE\n2 - VP8\n3 - V24P\n4 - V24M\n5,6,7 - temperature monitors\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_read_local_voltage,(void *)args);
  return 0; 
}

void *pt_read_local_voltage(void* args)
{
  read_local_voltage_t arg = *(read_local_voltage_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  XL3_Packet packet;
  read_local_voltage_args_t *packet_args = (read_local_voltage_args_t *) packet.payload;
  read_local_voltage_results_t *packet_results = (read_local_voltage_results_t *) packet.payload;
  packet.cmdHeader.packet_type = READ_LOCAL_VOLTAGE_ID;
  packet_args->voltage_select = arg.voltage_select;
  SwapLongBlock(packet_args,sizeof(read_local_voltage_args_t)/sizeof(uint32_t));
  do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
  SwapLongBlock(packet_results,sizeof(read_local_voltage_results_t)/sizeof(uint32_t));

  pt_printsend("Voltage #%d: %6.2f\n",arg.voltage_select,packet_results->voltage);

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

int hv_readback(char *buffer)
{
  hv_readback_t *args;
  args = malloc(sizeof(hv_readback_t));

  args->crate_num = 2;
  args->show_a = 0;
  args->show_b = 0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'a'){args->show_a = 1;
      }else if (words[1] == 'b'){args->show_b = 1;
      }else if (words[1] == 'h'){
        printsend("Usage: hv_readback -c [crate num (int)]"
            "-a (read out supply A) -b (read out supply B)\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  if (args->show_b == 0)
    args->show_a = 1;

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_hv_readback,(void *)args);
  return 0; 
}

void *pt_hv_readback(void* args)
{
  hv_readback_t arg = *(hv_readback_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  float voltage_a = 0, voltage_b = 0;
  float current_a = 0, current_b = 0;
  int i,nt = 1;

  XL3_Packet packet;
  hv_readback_results_t *packet_results = (hv_readback_results_t *) packet.payload;
  
  for (i=0;i<nt;i++){
    packet.cmdHeader.packet_type = HV_READBACK_ID;
    do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
    SwapLongBlock(packet_results,sizeof(hv_readback_results_t)/sizeof(uint32_t));
    voltage_a += packet_results->voltage_a;
    voltage_b += packet_results->voltage_b;
    current_a += packet_results->current_a;
    current_b += packet_results->current_b;
  }

  voltage_a /= (float) nt;
  voltage_b /= (float) nt;
  current_a /= (float) nt;
  current_b /= (float) nt;

  if (arg.show_a)
    pt_printsend("Supply A - Voltage: %6.3f volts, Current: %6.4f mA\n",voltage_a*300.0,current_a*10.0);
  if (arg.show_b)
    pt_printsend("Supply B - Voltage: %6.3f volts, Current: %6.4f mA\n",voltage_b*300.0,current_b*10.0);

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

int set_alarm_dac(char *buffer)
{
  set_alarm_dac_t *args;
  args = malloc(sizeof(set_alarm_dac_t));

  args->crate_num = 2;
  args->dac0 = 0xFFFFFFFF;
  args->dac1 = 0xFFFFFFFF;
  args->dac2 = 0xFFFFFFFF;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == '0'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->dac0 = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == '1'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->dac1 = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == '2'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->dac2 = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        printsend("Usage: hv_readback -c [crate num (int)]"
            "-a (read out supply A) -b (read out supply B)\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_set_alarm_dac,(void *)args);
  return 0; 
}

void *pt_set_alarm_dac(void* args)
{
  set_alarm_dac_t arg = *(set_alarm_dac_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  XL3_Packet packet;
  set_alarm_dac_args_t *packet_args = (set_alarm_dac_args_t *) packet.payload;
  
  packet_args->dacs[0] = arg.dac0;
  packet_args->dacs[1] = arg.dac1;
  packet_args->dacs[2] = arg.dac2;
  SwapLongBlock(packet_args,sizeof(set_alarm_dac_args_t)/sizeof(uint32_t));
  packet.cmdHeader.packet_type = SET_ALARM_DAC_ID;
  do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);


  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}


