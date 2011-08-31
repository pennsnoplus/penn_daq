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
  int thread_num = -1;
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
  pthread_create(new_thread,NULL,pt_load_relays,(void *)args);
  return 0; 
}

void *pt_load_relays(void *args)
{
  load_relays_t arg = *(load_relays_t *) args;
  free(args);

  int i,j;
  uint32_t result;
  for (i=0;i<16;i++){
    for (j=0;j<4;j++){
      if ((0x1<<j) & arg.pattern[i]){
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x2,&result,arg.crate_num);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0xa,&result,arg.crate_num);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x2,&result,arg.crate_num);
      }else{
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x0,&result,arg.crate_num);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x8,&result,arg.crate_num);
        xl3_rw(XL_RELAY_R + WRITE_REG, 0x0,&result,arg.crate_num);
      }
    }
  }
  usleep(1000);
  xl3_rw(XL_RELAY_R + WRITE_REG, 0x4,&result,arg.crate_num);


  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return;
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
      }else if (words[1] == 'a'){
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
  pthread_create(new_thread,NULL,pt_read_bundle,(void *)args);
  return 0; 
}

void *pt_read_bundle(void *args)
{
  read_bundle_t arg = *(read_bundle_t *) args;
  free(args);

  int errors = 0;
  uint32_t crate,slot,chan,gt8,gt16,cmos_es16,cgt_es16,cgt_es8,nc_cc;
  int cell;
  double qlx,qhs,qhl,tac;
  uint32_t pmtword[3];
  errors += xl3_rw(READ_MEM+arg.slot_num*FEC_SEL,0x0,pmtword,arg.crate_num);
  errors += xl3_rw(READ_MEM+arg.slot_num*FEC_SEL,0x0,pmtword+1,arg.crate_num);
  errors += xl3_rw(READ_MEM+arg.slot_num*FEC_SEL,0x0,pmtword+2,arg.crate_num);
  if (errors != 0){
    pt_printsend("There were %d errors reading out the bundles.\n",errors);
    xl3_lock[arg.crate_num] = 0;
    thread_done[arg.thread_num] = 1;
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
  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return;
}


