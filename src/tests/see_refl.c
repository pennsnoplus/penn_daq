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

  args->dac_value = 255;
  args->crate_num = 2;
  args->slot_mask = 0x0;
  args->pattern = 0xFFFFFFFF;
  args->frequency = 1000.0;
  args->update_db = 0;
  args->final_test = 0;

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
      }else if (words[1] == 'v'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->dac_value = atoi(words2);
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->pattern = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: see_refl -c [crate num (int)] "
            "-v [dac value (int)] -s [slot mask (hex)] "
            "-f [frequency (float)] -p [channel mask (hex)] "
            "-d (update database)\n");
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

  running_final_test = 1;

  char channel_results[32][100];
  int i,j,k;
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
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
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
          pt_printsend("Slot %d, channel %d. If good, hit enter. Otherwise type in a description of the problem (or just \"fail\") and hit enter.\n",i,j);
          read_from_tut(channel_results[j]);

          for (k=0;k<strlen(channel_results[j]);k++)
            if (channel_results[j][k] == '\n')
              channel_results[j][k] = '\0';

          if (strncmp(channel_results[j],"quit",4) == 0){
            pt_printsend("Quitting.\n");
            running_final_test = 0;
            unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
            return;
          }


        } // end pattern mask
      } // end loop over channels

      // clear chinj for this slot
      setup_chinj(arg.crate_num,(0x1<<i),0x0,arg.dac_value,&thread_fdset);

      // update the database
      if (arg.update_db){
        pt_printsend("updating the database\n");
        pt_printsend("updating slot %d\n",i);
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("see_refl"));

        int passflag = 1;
        JsonNode *all_channels = json_mkarray();
        for (j=0;j<32;j++){
          JsonNode *one_chan = json_mkobject();
          json_append_member(one_chan,"id",json_mknumber(j));
          if (strlen(channel_results[j]) != 0){
            passflag = 0;
            json_append_member(one_chan,"error",json_mkstring(channel_results[j]));
          }else{
            json_append_member(one_chan,"error",json_mkstring(""));
          }
          json_append_element(all_channels,one_chan);
        }
        json_append_member(newdoc,"channels",all_channels);
        json_append_member(newdoc,"pass",json_mkbool(passflag));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	
        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
        json_delete(newdoc); // Only have to delete the head node
      }
    } // end if slot mask
  } // end loop over slots

  disable_pulser();
  deselect_fecs(arg.crate_num,&thread_fdset);

  running_final_test = 0;

  pt_printsend("-----------------------------------------\n");
  if (errors)
    pt_printsend("There were %d errors.\n",errors);
  else
    pt_printsend("No errors.\n");
  pt_printsend("-----------------------------------------\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
  return;
}
