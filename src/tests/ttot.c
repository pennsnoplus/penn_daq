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
#define NUM_PEDS 500
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
  args->ecal = 0;

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
      }else if (words[1] == 'E'){
        if ((words2 = strtok(NULL, " ")) != NULL){
          args->ecal = 1;
          strcpy(args->ecal_id,words2);
        }
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
          if (arg.target_time < 9999){
            pt_printsend(">>> Warning: Time less than %d nsec",arg.target_time);
            tot_errors[i][j] = 1;
          }
        }else if(arg.target_time == 9999){
          pt_printsend(">>> Problem measuring time for this channel\n");
          tot_errors[i][j] = 2;
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
          json_append_member(one_chan,"errors",json_mknumber(tot_errors[slot][i]));
          json_append_element(channels,one_chan);
        }
        json_append_member(newdoc,"channels",channels);

        json_append_member(newdoc,"pass",json_mkbool(passflag));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        if (arg.ecal)
          json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
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
  args->ecal = 0;

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
      }else if (words[1] == 'E'){
        if ((words2 = strtok(NULL, " ")) != NULL){
          args->ecal = 1;
          strcpy(args->ecal_id,words2);
        }
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
  int tot_errors[16][32];
  int i,j,k,l;
  uint16_t rmp[8],vsi[8],rmpup[8],vli[8];
  uint16_t rmp_high[8],rmp_low[8];
  uint16_t chips_not_finished;
  int diff[32];
  uint32_t dac_nums[50],dac_values[50];
  int num_dacs;
  int result;

  // setup the mtc with the triggers going to the TUB
  int errors = setup_pedestals(0,60,100,0,(0x1<<arg.crate_num) | MSK_CRATE21,MSK_CRATE21);

  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      // set default values
      for (j=0;j<8;j++){
        rmpup[j] = RMPUP_DEFAULT;
        vsi[j] = VSI_DEFAULT;
        vli[j] = VLI_DEFAULT;
        rmp_high[j] = MAX_RMP_VALUE;
        rmp_low[j] = RMP_DEFAULT-10;
        rmp[j] = (int) (rmp_high[j] + rmp_low[j])/2;
      }

      // first check that if we make ttot as short as possible, triggers show up 
      num_dacs = 0;
      for (j=0;j<8;j++){
        dac_nums[num_dacs] = d_rmpup[j];
        dac_values[num_dacs] = rmpup[j];
        num_dacs++;
        dac_nums[num_dacs] = d_vli[j];
        dac_values[num_dacs] = vli[j];
        num_dacs++;
        dac_nums[num_dacs] = d_rmp[j];
        dac_values[num_dacs] = rmp_low[j];
        num_dacs++;
        dac_nums[num_dacs] = d_vsi[j];
        dac_values[num_dacs] = vsi[j];
        num_dacs++;
      }
      multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset);
      for (k=0;k<32;k++){
        result = disc_check_ttot(arg.crate_num,i,(0x1<<k),MAX_TIME,diff,&thread_fdset);
        tot_errors[i][k] = 0;
        if (diff[k] == 1){
          pt_printsend("Error - Not getting TUB triggers on channel %d!\n",k);
          tot_errors[i][k] = 2;
        }
      }

      // load default values
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

      pt_printsend("Setting ttot for crate/board %d %d, target time %d\n",arg.crate_num,i,arg.target_time);
      chips_not_finished = 0xFF;

      // loop until all ttot measurements are larger than target ttime
      while(chips_not_finished){
        //for (j=0;j<8;j++)
        //if ((0x1<<j) & chips_not_finished)
        //pt_printsend("hello");

        // measure ttot for all chips
        result = disc_check_ttot(arg.crate_num,i,0xFFFFFFFF,arg.target_time,diff,&thread_fdset);
        //result = disc_m_ttot(arg.crate_num,(0x1<<i),150,alltimes,&thread_fdset);
        //for (j=0;j<8;j++){
        //  diff[4*j+0] = alltimes[i*32+j*4+0] - arg.target_time;
        //  diff[4*j+1] = alltimes[i*32+j*4+1] - arg.target_time;
        //  diff[4*j+2] = alltimes[i*32+j*4+2] - arg.target_time;
        //  diff[4*j+3] = alltimes[i*32+j*4+3] - arg.target_time;
        //}

        // loop over disc chips
        for (j=0;j<8;j++){
          if ((0x1<<j) & chips_not_finished){

            // check if above or below
            if ((diff[4*j+0] > 0) && (diff[4*j+1] > 0) && (diff[4*j+2] > 0)
                && (diff[4*j+3] > 0)){
              //pt_printsend("above\n");
              rmp_high[j] = rmp[j];

              // if we have narrowed it down to the first setting that works, we are done
              if ((rmp[j] - rmp_low[j]) == 1){
                pt_printsend("Chip %d finished\n",j);
                chips_not_finished &= ~(0x1<<j);
                allrmps[i][j] = rmp[j];
                allvsis[i][j] = vsi[j];
              }
            }else{
              //pt_printsend("below\n");
              rmp_low[j] = rmp[j];
              if (rmp[j] == MAX_RMP_VALUE){
                if (vsi[j] > MIN_VSI_VALUE){
                  rmp_high[j] = MAX_RMP_VALUE;
                  rmp_low[j] = RMP_DEFAULT-10;
                  vsi[j] -= 2;
                  //pt_printsend("%d - vsi: %d\n",j,vsi[j]);
                }else{
                  // out of bounds, end loop with error
                  pt_printsend("RMP/VSI is too big for disc chip %d! (%d %d)\n",j,rmp[j],vsi[j]);
                  pt_printsend("Aborting slot %d setup.\n",i);
                  tot_errors[i][j*4+0] = 1;
                  tot_errors[i][j*4+1] = 1;
                  tot_errors[i][j*4+2] = 1;
                  tot_errors[i][j*4+3] = 1;
                  for (l=0;l<8;l++)
                    if (chips_not_finished & (0x1<<l)){
                      pt_printsend("Slot %d Chip %d\tRMP/VSI: %d %d <- unfinished\n",i,l,rmp[l],vsi[l]);
                      allrmps[i][l] = rmp[l];
                      allvsis[i][l] = vsi[l];
                    }
                  chips_not_finished = 0x0;
                }
              }else if (rmp[j] == rmp_high[j]){
                // in case it screws up and fails after it succeeded already
                rmp_high[j]++;
              }
            }

            rmp[j] = (int) ((float) (rmp_high[j] + rmp_low[j])/2.0 + 0.5);

          } // end if this chip not finished
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

      // now get the final timing measurements
      num_dacs = 0;
      for (j=0;j<8;j++){
        dac_nums[num_dacs] = d_rmp[j];
        dac_values[num_dacs] = allrmps[i][j];
        num_dacs++;
        dac_nums[num_dacs] = d_vsi[j];
        dac_values[num_dacs] = allvsis[i][j];
        num_dacs++;
      }
      multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset);

      result = disc_m_ttot(arg.crate_num,(0x1<<i),150,alltimes,&thread_fdset);

      pt_printsend("Final timing measurements:\n");
      for (j=0;j<8;j++){
        pt_printsend("Chip %d (RMP/VSI %d %d) Times:\t%d\t%d\t%d\t%d\n",
            j,rmp[j],vsi[j],alltimes[i*32+j*4+0],alltimes[i*32+j*4+1],
            alltimes[i*32+j*4+2],alltimes[i*32+j*4+3]);
      }

      if (arg.update_db){
        pt_printsend("updating the database\n");
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("set_ttot"));
        json_append_member(newdoc,"targettime",json_mknumber((double)arg.target_time));

        JsonNode *all_chips = json_mkarray();
        int passflag = 1;
        int k;
        for (k=0;k<8;k++){
          JsonNode *one_chip = json_mkobject();
          json_append_member(one_chip,"rmp",json_mknumber((double) allrmps[i][k]));
          json_append_member(one_chip,"vsi",json_mknumber((double) allvsis[i][k]));

          JsonNode *all_chans = json_mkarray();
          for (j=0;j<4;j++){
            JsonNode *one_chan = json_mkobject();
            if (tot_errors[i][j]> 0)
              passflag = 0;
            json_append_member(one_chan,"id",json_mknumber((double) k*4+j));
            json_append_member(one_chan,"time",json_mknumber((double) alltimes[i*32+k*4+j]));
            json_append_member(one_chan,"errors",json_mknumber(tot_errors[i][j]));
            json_append_element(all_chans,one_chan);
          }
          json_append_member(one_chip,"channels",all_chans);

          json_append_element(all_chips,one_chip);
        }
        json_append_member(newdoc,"chips",all_chips);

        json_append_member(newdoc,"pass",json_mkbool(passflag));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	
        if (arg.ecal)
          json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
        json_delete(newdoc); // head node needs deleting
      }
    } // if in slot mask
  } // end loop over slots

  pt_printsend("Set ttot complete\n");
  pt_printsend("**************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}


int disc_m_ttot(int crate, uint32_t slot_mask, int start_time, uint16_t *disc_times, fd_set *thread_fdset)
{
  int increment = 1;
  int time;
  uint32_t chan_done_mask;
  float real_delay;
  uint32_t init[32],fin[32];
  int i,j,k;

  for (i=0;i<16;i++){
    if ((0x1<<i) & slot_mask){
      int result = set_crate_pedestals(crate,0x1<<i,0xFFFFFFFF,thread_fdset);
      chan_done_mask = 0x0;
      time = start_time;
      while (chan_done_mask != 0xFFFFFFFF){
        // set up gt delay
        real_delay = set_gt_delay((float) time);
        while ((real_delay > (float) time) || ((real_delay + (float) increment) < (float) time)){
          printf("got %f instead of %f, trying again\n",real_delay,(float) time);
          real_delay = set_gt_delay((float) time);
        }
        // get the cmos count before sending pulses
        result = get_cmos_total_count(crate,i,init,thread_fdset);
        // send some pulses
        multi_softgt(NUM_PEDS);
        //now read out the count again to get the rate
        result = get_cmos_total_count(crate,i,fin,thread_fdset);
        for (j=0;j<32;j++){
          fin[j] -= init[j];
          //pt_printsend("for %d at time %d, got %d of %d\n",j,time,fin[j],2*NUM_PEDS);
          // check if we got all the pedestals from the TUB too
          if ((fin[j] >= 2*NUM_PEDS) && ((0x1<<j) & ~chan_done_mask)){
            chan_done_mask |= (0x1<<j); 
            disc_times[i*32+j] = (int)real_delay+TUB_DELAY;
          }
        }
        if (chan_done_mask == 0xFFFFFFFF)
          break;
        if (time >= MAX_TIME){
          for (k=0;k<32;k++){
            if ((0x1<<k) & ~chan_done_mask)
              disc_times[i*32+k] = time+TUB_DELAY;
            chan_done_mask = 0xFFFFFFFF;
          }
        }else{
          if (((int) (real_delay + 0.5) + increment) > time)
            time = (int) (real_delay + 0.5) + increment;
          if (time > MAX_TIME)
            time = MAX_TIME;
        }
      } // for time<=MAX_TIME

      // now that we got our times, check each channel one by one to ensure it was
      // working on its own
      for (j=0;j<32;j++){
        result = set_crate_pedestals(crate,0x1<<i,0x1<<j,thread_fdset);
        // if it worked before at time-tub_delay, it should work for time-tub_delay+50
        real_delay = set_gt_delay((float) disc_times[i*32+j]-TUB_DELAY+50);
        while (real_delay < ((float) disc_times[i*32+j] - TUB_DELAY + 50 - 5))
        {
          printf("2 - got %f instead of %f, trying again\n",real_delay,(float) disc_times[i*32+j] - TUB_DELAY + 50);
          real_delay = set_gt_delay((float) disc_times[i*32+j]-TUB_DELAY+50);
        }

        result = get_cmos_total_count(crate,i,init,thread_fdset);
        multi_softgt(NUM_PEDS);
        result = get_cmos_total_count(crate,i,fin,thread_fdset);
        fin[j] -= init[j];
        if (fin[j] < 2*NUM_PEDS){
          // we didn't get the peds without the other channels enabled
          pt_printsend("Error channel %d - pedestals went away after other channels turned off!\n",j);
          disc_times[i*32+j] = 9999;
        }
      }

    } // end if slot mask
  } // end loop over slots
  return 0;
}

int disc_check_ttot(int crate, int slot_num, uint32_t chan_mask, int goal_time, int *diff, fd_set *thread_fdset)
{
  float real_delay;
  uint32_t init[32],fin[32];
  int i,j,k;

  // initialize array
  for (i=0;i<32;i++)
    diff[i] = 0;

  int result = set_crate_pedestals(crate,0x1<<slot_num,chan_mask,thread_fdset);

  // measure it twice to make sure we are good
  for (i=0;i<2;i++){
    real_delay = set_gt_delay((float) goal_time - TUB_DELAY);
    while (real_delay < ((float) goal_time - TUB_DELAY - 5))
    {
      printf("3 - got %f instead of %f, trying again\n",real_delay,(float)goal_time - TUB_DELAY);
      real_delay = set_gt_delay((float) goal_time - TUB_DELAY);
    }

    // get the cmos count before sending pulses
    result = get_cmos_total_count(crate,slot_num,init,thread_fdset);
    // send some pulses
    multi_softgt(NUM_PEDS);
    // now read out the count again to get the rate
    result = get_cmos_total_count(crate,slot_num,fin,thread_fdset);
    for (k=0;k<32;k++){
      fin[k] -= init[k];
      // check if we got all the peds from the TUB too
      if (fin[k] < 2*NUM_PEDS){
        // we didnt get all the peds, so ttot is longer than our target time
        if (i==0)
          diff[k] = 1;
      }else{
        //pt_printsend("%d was short. Got %d out of %d (%d before, %d after)\n",k,fin[k],2*NUM_PEDS,init[k],fin[k]+init[k]);
        // if its shorter either time, flag it as too short
        diff[k] = 0;
      }
    }
  }
  return 0;
}
