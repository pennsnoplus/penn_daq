#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"
#include "mtc_registers.h"
#include "dac_number.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_utils.h"
#include "ttot.h"

#define MAX_TIME 1100
#define NUM_PEDS 20
#define TUB_DELAY 60

#define RMP_DEFAULT 120
#define RMPUP_DEFAULT 115
#define VSI_DEFAULT 120
#define VLI_DEFAULT 120
#define MAX_RMP_VALUE 180
#define MIN_VSI_VALUE 50

int get_ttot(char *buffer)
{
  get_ttot_t *args;
  args = malloc(sizeof(get_ttot_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->target_time = 400;
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
      }else if (words[1] == 't'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->target_time = atoi(words2);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: get_ttot -c [crate num (int)] "
            "-s [slot mask (hex)] -t [target time] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_get_ttot,(void *)args);
  return 0; 
}


void *pt_get_ttot(void *args)
{
  get_ttot_t arg = *(get_ttot_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  uint16_t times[32*16];
  int tot_errors[16][32];

  // setup the mtc with the triggers going to the TUB
  int errors = setup_pedestals(0,60,100,0,(0x1<<arg.crate_num) | MSK_CRATE21,MSK_CRATE21);

  int result = disc_m_ttot(arg.crate_num,arg.slot_mask,150,times,&thread_fdset);

  // print out results
  pt_printsend("Crate\t Slot\t Channel\t Time:\n");
  int i,j;
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      for (j=0;j<32;j++){
        tot_errors[i][j] = 0;
        pt_printsend("%d\t %d\t %d\t %d",arg.crate_num,i,j,times[i*32+j]);
        if (arg.target_time > times[i*32+j]){
          pt_printsend(">>> Warning: Time less than %d nsec",arg.target_time);
          tot_errors[i][j] = 1;
        }
        pt_printsend("\n");
      }
    }
  }

  if (arg.update_db){
    printsend("updating the database\n");
    int slot;
    for (slot=0;slot<16;slot++){
      if ((0x1<<slot) & arg.slot_mask){
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("get_ttot"));
        json_append_member(newdoc,"targettime",json_mknumber((double)arg.target_time));

        JsonNode *channels = json_mkarray();
        int passflag = 1;
        for (i=0;i<32;i++){
          if (tot_errors[slot][i] == 1)
            passflag = 0;
          JsonNode *one_chan = json_mkobject();
          json_append_member(one_chan,"id",json_mknumber((double) i));
          json_append_member(one_chan,"time",json_mknumber((double) times[slot*32+i]));
          json_append_member(one_chan,"errors",json_mkbool(tot_errors[slot][i]));
          json_append_element(channels,one_chan);
        }
        json_append_member(newdoc,"channels",channels);

        json_append_member(newdoc,"pass",json_mkbool(passflag));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc); // delete the head ndoe
      }
    }
  }

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

int set_ttot(char *buffer)
{
  set_ttot_t *args;
  args = malloc(sizeof(set_ttot_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->target_time = 400;
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
      }else if (words[1] == 't'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->target_time = atoi(words2);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: set_ttot -c [crate num (int)] "
            "-s [slot mask (hex)] -t [target time] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_set_ttot,(void *)args);
  return 0; 
}


void *pt_set_ttot(void *args)
{
  set_ttot_t arg = *(set_ttot_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  uint16_t allrmps[16][8],allvsis[16][8],alltimes[16*32];
  int tot_errors[16][8];
  int i,j,k,l;
  uint16_t rmp[8],vsi[8],rmpup[8],vli[8];
  uint16_t chips_not_finished;
  int32_t diff[32];
  uint32_t dac_nums[50],dac_values[50];
  int num_dacs;

  // setup the mtc with the triggers going to the TUB
  int errors = setup_pedestals(0,60,100,0,(0x1<<arg.crate_num) | MSK_CRATE21,MSK_CRATE21);

  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      // set default values
      for (j=0;j<8;j++){
        rmpup[j] = RMPUP_DEFAULT;
        rmp[j] = RMP_DEFAULT-10;
        vsi[j] = VSI_DEFAULT;
        vli[j] = VLI_DEFAULT;
      }
      // load default values
      num_dacs = 0;
      for (j=0;j<8;j++){
        tot_errors[i][j] = 0;
        dac_nums[num_dacs] = d_rmpup[j];
        dac_values[num_dacs] = rmpup[j];
        num_dacs++;
        dac_nums[num_dacs] = d_rmp[j];
        dac_values[num_dacs] = rmp[j];
        num_dacs++;
        dac_nums[num_dacs] = d_vsi[j];
        dac_values[num_dacs] = vsi[j];
        num_dacs++;
        dac_nums[num_dacs] = d_vli[j];
        dac_values[num_dacs] = vli[j];
        num_dacs++;
      }
      multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset);

      pt_printsend("Setting ttot for crate/board %d %d, target time %d\n",arg.crate_num,i,arg.target_time);
      chips_not_finished = 0xFF;
      while(chips_not_finished){
        int result = disc_m_ttot(arg.crate_num,0x1<<i,150,alltimes,&thread_fdset);
        // loop over disc chips
        for (j=0;j<8;j++){
          // loop over channels in chip
          for (k=0;k<4;k++)
            diff[4*j+k] = alltimes[i*32+j*4+k] - arg.target_time;

          // if diffs all positive, that chip is finished
          if ((diff[4*j+0] > 0) && (diff[4*j+1] > 0) && (diff[4*j+2] > 0) && (diff[4*j+3] > 0) && ((0x1<<j) & chips_not_finished)){
            chips_not_finished &= ~(0x1<<j);
            pt_printsend("Fit channel %d (RMP/VSI %d %d) Times:\t%d\t%d\t%d\t%d\n",
                j,rmp[j],vsi[j],alltimes[i*32+j*4+0],alltimes[i*32+j*4+1],
                alltimes[i*32+j*4+2],alltimes[i*32+j*4+3]);
            allrmps[i][j] = rmp[j];
            allvsis[i][j] = vsi[j];
          }else{
            // if not done, adjust DACs
            if ((rmp[j] <= MAX_RMP_VALUE) && (vsi[j] > MIN_VSI_VALUE) && ((0x1<<j) & chips_not_finished))
              rmp[j]++;
            if ((rmp[j] > MAX_RMP_VALUE) && (vsi[j] > MIN_VSI_VALUE) && ((0x1<<j) & chips_not_finished)){
              rmp[j] = RMP_DEFAULT-10;
              vsi[j] -= 2;
            }
            if ((vsi[j] <= MIN_VSI_VALUE) && ((0x1<<j) & chips_not_finished)){
              // out of bounds, end loop with error
              pt_printsend("RMP/VSI is too big for disc chip %d! (%d %d)\n",j,rmp[j],vsi[j]);
              pt_printsend("Aborting slot %d setup.\n",i);
              tot_errors[i][j] = 1;
              for (l=0;l<8;l++)
                if (chips_not_finished & (0x1<<l))
                  pt_printsend("Slot %d Chip %d\tRMP/VSI: %d %d <- unfinished\n",i,j,rmp[l],vsi[l]);
              goto end; // oh the horror!
            }
          }
        } // end loop over disc chips

        // load new values
        num_dacs = 0;
        for (j=0;j<8;j++){
          dac_nums[num_dacs] = d_rmp[j];
          dac_values[num_dacs] = rmp[j];
          num_dacs++;
          dac_nums[num_dacs] = d_vsi[j];
          dac_values[num_dacs] = vsi[j];
          num_dacs++;
        }
        multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset);

      } // end while chips_not_finished

end:
      if (arg.update_db){
        pt_printsend("updating the database\n");
        int slot;
        for (slot=0;slot<16;slot++){
          if ((0x1<<slot) & arg.slot_mask){
            JsonNode *newdoc = json_mkobject();
            json_append_member(newdoc,"type",json_mkstring("set_ttot"));
            json_append_member(newdoc,"targettime",json_mknumber((double)arg.target_time));

            JsonNode *all_chips = json_mkarray();
            int passflag = 1;
            for (i=0;i<8;i++){
              if (tot_errors[slot][i] == 1)
                passflag = 0;
              JsonNode *one_chip = json_mkobject();
              json_append_member(one_chip,"rmp",json_mknumber((double) allrmps[slot][i]));
              json_append_member(one_chip,"vsi",json_mknumber((double) allrmps[slot][i]));
              json_append_member(one_chip,"errors",json_mkbool(tot_errors[slot][i]));
              JsonNode *one_chan = json_mkarray();
              for (j=0;j<4;j++)
                json_append_element(one_chan,json_mknumber((double) alltimes[slot*32+i*4+j]));
              json_append_member(one_chip,"times",one_chan);
              json_append_element(all_chips,one_chip);
            }
            json_append_member(newdoc,"chips",all_chips);

            json_append_member(newdoc,"pass",json_mkbool(passflag));
            if (arg.final_test)
              json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
            post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
            json_delete(newdoc); // head node needs deleting
          }
        }
      }
    } // if in slot mask
  } // end loop over slots

  pt_printsend("Set ttot complete\n");
  pt_printsend("**************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}


int disc_m_ttot(int crate, uint32_t slot_mask, int start_time, uint16_t *disc_times, fd_set *thread_fdset)
{
  int increment = 10;
  int time;
  uint32_t chan_done_mask;
  float real_delay;
  uint32_t init[32],fin[32];
  int i,j;

  for (i=0;i<16;i++){
    if ((0x1<<i) & slot_mask){
      int result = set_crate_pedestals(crate,0x1<<i,0xFFFFFFFF,thread_fdset);
      chan_done_mask = 0x0;
      for (time = start_time;time<=MAX_TIME;time+=increment){
        // set up gt delay
        real_delay = set_gt_delay((float) time);
        // get the cmos count before sending pulses
        result = get_cmos_total_count(crate,i,init,thread_fdset);
        // send some pulses
        multi_softgt(NUM_PEDS);
        //now read out the count again to get the rate
        result = get_cmos_total_count(crate,i,fin,thread_fdset);
        for (j=0;j<32;j++){
          fin[j] -= init[j];
          // check if we got all the pedestals from the TUB too
          if ((fin[j] >= 2*NUM_PEDS) && !(chan_done_mask & (0x1<<j))){
            chan_done_mask |= 0x1<<j;
            disc_times[i*32+j] = time+TUB_DELAY;
          }
        }
        if (chan_done_mask == 0xFFFFFFFF)
          break;
        if (time == MAX_TIME)
          for (j=0;j<32;j++)
            if ((0x1<<j) & !chan_done_mask)
              disc_times[i*32+j] = time+TUB_DELAY;

      } // for time<=MAX_TIME
    } // end if slot mask
  } // end loop over slots
  return 0;
}
