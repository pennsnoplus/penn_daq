#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "xl3_registers.h"
#include "unpack_bundles.h"

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "xl3_utils.h"
#include "fec_cmds.h"
#include "chinj_scan.h"

int frw(char *buffer, int rw)
{
  frw_t *args;
  args = malloc(sizeof(frw_t));

  args->rw = rw;
  args->crate_num = 2;
  args->slot_num = 8;
  args->register_num = 0;
  args->data = 0x0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->slot_num = atoi(words2);
      }else if (words[1] == 'r'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->register_num = atoi(words2);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->data = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        if (rw == 0)
          pt_printsend("Usage: fr -c [crate num (int)] -s [slot num (int)] -r [register number (int)]\n");
        else if (rw == 1)
          pt_printsend("Usage: fw -c [crate num (int)] -s [slot num (int)] -r [register number (int)] -d [data (hex)]\n");
        pt_printsend("type \"help fec_registers\" to get "
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
  pthread_create(new_thread,NULL,pt_frw,(void *)args);
  return 0; 
}

void *pt_frw(void *args)
{
  frw_t arg = *(frw_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  if (arg.register_num > 276){
    pt_printsend("Not a valid register.\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  uint32_t result;
  if (arg.rw == 0){
    int errors = xl3_rw(fec_reg_addresses[arg.register_num] + FEC_SEL*arg.slot_num + READ_REG,0x0, &result, arg.crate_num,&thread_fdset);
    pt_printsend("%08x\n",result);
    //pt_printsend("Read out %08x from register %d (%08x)\n",result,arg.register_num,xl3_reg_addresses[arg.register_num]);
  }else{
    int errors = xl3_rw(fec_reg_addresses[arg.register_num] + FEC_SEL*arg.slot_num + WRITE_REG,arg.data, &result, arg.crate_num,&thread_fdset);
    pt_printsend("%08x\n",result);
    //pt_printsend("Wrote %08x to register %d (%08x)\n",arg.data,arg.register_num,xl3_reg_addresses[arg.register_num]);
  }

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
  return;
}



int load_relays(char *buffer)
{
  load_relays_t *args;
  args = malloc(sizeof(load_relays_t));

  args->crate_num = 2;
  int i;
  for (i=0;i<16;i++)
    args->pattern[i] = 0xf;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          for (i=0;i<16;i++)
            args->pattern[i] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '0'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[0] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '1'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[1] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '2'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[2] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '3'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[3] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '4'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[4] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '5'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[5] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '6'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[6] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '7'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[7] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '8'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[8] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == '9'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[9] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'a'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[10] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'b'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[11] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[12] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[13] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'e'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[14] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'f'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->pattern[15] = strtoul(words2,(char **) NULL, 16);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: load_relays -c [crate num (int)] "
            "-p [paddle cards (all cards) (hex)] -0...-f [paddle cards (for this card) (hex)]\n");
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
  pthread_create(new_thread,NULL,pt_load_relays,(void *)args);
  return 0; 
}

void *pt_load_relays(void *args)
{
  load_relays_t arg = *(load_relays_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int i,j;
  uint32_t result;
  for (i=0;i<16;i++){
    for (j=0;j<4;j++){
      if ((0x1<<j) & arg.pattern[i]){
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x2,&result,arg.crate_num,&thread_fdset);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0xa,&result,arg.crate_num,&thread_fdset);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x2,&result,arg.crate_num,&thread_fdset);
      }else{
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x0,&result,arg.crate_num,&thread_fdset);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x8,&result,arg.crate_num,&thread_fdset);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x0,&result,arg.crate_num,&thread_fdset);
      }
    }
  }
  usleep(1000);
  xl3_rw(XL_RELAY_R + WRITE_REG, 0x4,&result,arg.crate_num,&thread_fdset);


  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

int read_bundle(char *buffer)
{
  read_bundle_t *args;
  args = malloc(sizeof(read_bundle_t));

  args->crate_num = 2;
  args->slot_num = 7;
  args->quiet = 0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->slot_num = atoi(words2);
      }else if (words[1] == 'q'){
        args->quiet = 1;
      }else if (words[1] == 'h'){
        pt_printsend("Usage: read_bundle -c [crate num (int)] "
            "-s [slot num (int)] -q (quiet mode)\n");
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
  pthread_create(new_thread,NULL,pt_read_bundle,(void *)args);
  return 0; 
}

void *pt_read_bundle(void *args)
{
  read_bundle_t arg = *(read_bundle_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int errors = 0;
  uint32_t crate,slot,chan,gt8,gt16,cmos_es16,cgt_es16,cgt_es8,nc_cc;
  int cell;
  double qlx,qhs,qhl,tac;
  uint32_t pmtword[3];
  errors += xl3_rw(READ_MEM+arg.slot_num*FEC_SEL,0x0,pmtword,arg.crate_num,&thread_fdset);
  errors += xl3_rw(READ_MEM+arg.slot_num*FEC_SEL,0x0,pmtword+1,arg.crate_num,&thread_fdset);
  errors += xl3_rw(READ_MEM+arg.slot_num*FEC_SEL,0x0,pmtword+2,arg.crate_num,&thread_fdset);
  if (errors != 0){
    pt_printsend("There were %d errors reading out the bundles.\n",errors);
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }
  pt_printsend("%08x %08x %08x\n",pmtword[0],pmtword[1],pmtword[2]);
  if (arg.quiet == 0){
    crate = (uint32_t) UNPK_CRATE_ID(pmtword);
    slot = (uint32_t)  UNPK_BOARD_ID(pmtword);
    chan = (uint32_t)  UNPK_CHANNEL_ID(pmtword);
    cell = (int) UNPK_CELL_ID(pmtword);
    gt8 = (uint32_t)   UNPK_FEC_GT8_ID(pmtword);
    gt16 = (uint32_t)  UNPK_FEC_GT16_ID(pmtword);
    cmos_es16 = (uint32_t) UNPK_CMOS_ES_16(pmtword);
    cgt_es16 = (uint32_t)  UNPK_CGT_ES_16(pmtword);
    cgt_es8 = (uint32_t)   UNPK_CGT_ES_24(pmtword);
    nc_cc = (uint32_t) UNPK_NC_CC(pmtword);
    qlx = (double) MY_UNPK_QLX(pmtword);
    qhs = (double) UNPK_QHS(pmtword);
    qhl = (double) UNPK_QHL(pmtword);
    tac = (double) UNPK_TAC(pmtword);
    pt_printsend("crate %08x, slot %08x, chan %08x, cell %d, gt8 %08x, gt16 %08x, cmos_es16 %08x,"
        " cgt_es16 %08x, cgt_es8 %08x, nc_cc %08x, qlx %6.1f, qhs %6.1f, qhl %6.1f, tac %6.1f\n",
        crate,slot,chan,cell,gt8,
        gt16,cmos_es16,cgt_es16,cgt_es8,nc_cc,qlx,qhs,qhl,tac);
  }
  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

int cmd_setup_chinj(char *buffer)
{
  cmd_setup_chinj_t *args;
  args = malloc(sizeof(cmd_setup_chinj_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->default_ch_mask = 0xFFFFFFFF;
  args->dacvalue = 0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->slot_mask = strtoul(words2,(char**)NULL, 16);
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->default_ch_mask = strtoul(words2,(char**)NULL, 16);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->dacvalue = atoi(words2);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: setup_chinj -c [crate num (int)] "
            "-s [slot mask (hex)] -p [channel mask (hex)] -d [dac value (int)]\n");
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
  pthread_create(new_thread,NULL,pt_cmd_setup_chinj,(void *)args);
  return 0; 
}

void *pt_cmd_setup_chinj(void *args)
{
  cmd_setup_chinj_t arg = *(cmd_setup_chinj_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  setup_chinj(arg.crate_num, arg.slot_mask, arg.default_ch_mask, arg.dacvalue, &thread_fdset);
  pt_printsend("Set up charge injection\n");

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}


int cmd_load_dac(char *buffer)
{
  cmd_load_dac_t *args;
  args = malloc(sizeof(cmd_load_dac_t));

  args->crate_num = 2;
  args->slot_num = 13;
  args->dac_num = 0;
  args->dac_value = 0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->slot_num = atoi(words2);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->dac_num = atoi(words2);
      }else if (words[1] == 'v'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->dac_value = atoi(words2);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: load_dac -c [crate num (int)] "
            "-s [slot num (int)] -d [dac num (int)] -v [dac value (int)]\n");
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
  pthread_create(new_thread,NULL,pt_cmd_load_dac,(void *)args);
  return 0; 
}

void *pt_cmd_load_dac(void *args)
{
  cmd_load_dac_t arg = *(cmd_load_dac_t *) args;
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);
  
  loadsDac(arg.dac_num, arg.dac_value, arg.crate_num, arg.slot_num,&thread_fdset);

  pt_printsend("Dac loaded\n");

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}


