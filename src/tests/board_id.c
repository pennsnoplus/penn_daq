#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "board_id.h"

int board_id(char *buffer)
{
  board_id_t *args;
  args = malloc(sizeof(board_id_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->slot_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        printsend("Usage: board_id -c [crate num (int)] "
            "-s [slot mask (hex)]\n");
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
  pthread_create(new_thread,NULL,pt_board_id,(void *)args);
  return 0; 
}


void *pt_board_id(void *args)
{
  board_id_t arg = *(board_id_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  XL3_Packet packet;
  board_id_args_t *packet_args = (board_id_args_t *) packet.payload;
  board_id_results_t *packet_results = (board_id_results_t *) packet.payload;

  pt_printsend("Starting board_id:\n");
  pt_printsend("SLOT ID: MB     DB1     DB2     DB3     DB4     HVC\n");

  int i,j;
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      pt_printsend("%d      ",i);
      for (j=1;j<7;j++){
        packet.cmdHeader.packet_type = BOARD_ID_READ_ID;
        packet_args->slot = i;
        packet_args->chip = j;
        packet_args->reg = 15;
        SwapLongBlock(packet_args,sizeof(board_id_args_t)/sizeof(uint32_t));
        do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
        SwapLongBlock(packet_results,sizeof(board_id_results_t)/sizeof(uint32_t));
        pt_printsend("0x%04x ",packet_results->id);
      }
      pt_printsend("\n");
    }
  }
  pt_printsend("**********************************\n");

  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return 0;
}

