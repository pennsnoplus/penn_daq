#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"
#include "xl3_registers.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "mtc_utils.h"
#include "daq_utils.h"
#include "chinj_scan.h"
#include "see_refl.h"

#define MAX_ERRORS 50

int see_refl(char *buffer)
{
  see_refl_t *args;
  args = malloc(sizeof(see_refl_t));

  args->dac_value = 0;
  args->crate_num = 2;
  args->pattern = 0xFFFFFFFF;
  args->debug = 0;
  args->frequency = 1000.0;
  args->start_slot = 0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->start_slot = atoi(words2);
      }else if (words[1] == 'v'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->dac_value = atoi(words2);
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->pattern = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'd'){args->debug = 1;
      }else if (words[1] == 'h'){
        printsend("Usage: see_refl -c [crate num (int)] "
            "-v [dac value (int)] -s [starting slot (int)] "
            "-f [frequency (float)] -p [channel mask (hex)] "
            "-d (debugging on)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_see_refl,(void *)args);
  return 0; 
}


void *pt_see_refl(void *args)
{
  see_refl_t arg = *(see_refl_t *) args; 
  free(args);

  char comments[100];
  int i,j;
  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  pt_printsend("Starting See Reflection Test\n");

  // set up pulser
  int errors = setup_pedestals(arg.frequency, DEFAULT_PED_WIDTH, DEFAULT_GT_DELAY,0,
      (0x1<<arg.crate_num),(0x1<<arg.crate_num) | MSK_TUB);
  if (errors){
    pt_printsend("Error setting up MTC for pedestals. Exiting\n");
    unset_ped_crate_mask(MASKALL);
    unset_gt_crate_mask(MASKALL);
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // loop over slots
  for (i=arg.start_slot;i<16;i++){
    // loop over channels
    for (j=0;j<32;j++){
      if ((0x1<<j) & arg.pattern){
        uint32_t temp_pattern = 0x1<<j;
        
        // turn on pedestals for just this one channel
        errors += set_crate_pedestals(arg.crate_num,(0x1<<i),temp_pattern,&thread_fdset);
        if (errors){
          pt_printsend("Error setting up pedestals, Slot %d, channel %d.\n",i,j);
          if (errors > MAX_ERRORS){
            pt_printsend("Too many errors. Exiting\n");
            disable_pulser();
            unset_ped_crate_mask(MASKALL);
            unset_gt_crate_mask(MASKALL);
            unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
            return;
          }
        }

        // set up charge injection for this channel
        setup_chinj(arg.crate_num,(0x1<<i),temp_pattern,arg.dac_value,&thread_fdset);
        // wait until something is typed
        pt_printsend("Slot %d, channel %d. Hit return for next channel.\n",i,j);
        read_from_tut(comments);
      } // end pattern mask
    } // end loop over channels

    // clear chinj for this slot
    setup_chinj(arg.crate_num,(0x1<<i),0x0,arg.dac_value,&thread_fdset);
  } // end loop over slots
  
  disable_pulser();
  deselect_fecs(arg.crate_num,&thread_fdset);

  pt_printsend("-----------------------------------------\n");
  if (errors)
    pt_printsend("There were %d errors.\n",errors);
  else
    pt_printsend("No errors.\n");
  pt_printsend("-----------------------------------------\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
  return;
}
