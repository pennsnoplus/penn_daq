#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_utils.h"
#include "mtc_init.h"

int mtc_init(char *buffer)
{
  int xilinx_load = 0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'x'){
        xilinx_load = 1;
      }else if (words[1] == 'h'){
        printsend("Usage: mtc_init -x (load xilinx)\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (sbc_lock != 0){
    // the sbc is locked, we cant do this right now
    return -1;
  }
  if (sbc_connected == 0){
    printsend("SBC is not connected! Aborting\n");
    return 0;
  }
  // spawn a thread to do it
  sbc_lock = 1;

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
  args[1] = (uint32_t) xilinx_load;
  pthread_create(new_thread,NULL,pt_mtc_init,(void *)args);
  return 0; 
}

void *pt_mtc_init(void *args)
{
  int thread_num = ((uint32_t *) args)[0];
  int xilinx_load = ((uint32_t *) args)[1];
  free(args);

  if (xilinx_load)
    mtc_xilinx_load();

  sbc_lock = 0;
  thread_done[thread_num] = 1;
}
