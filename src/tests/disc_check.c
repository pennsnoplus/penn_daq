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
#include "daq_utils.h"
#include "mtc_utils.h"
#include "xl3_utils.h"
#include "fec_utils.h"
#include "disc_check.h"

#define MAX_PER_PACKET 5000

int disc_check(char *buffer)
{
  disc_check_t *args;
  args = malloc(sizeof(disc_check_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->num_pedestals = 100000;
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
      }else if (words[1] == 'n'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->num_pedestals = atoi(words2);
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
        printsend("Usage: disc_check -c [crate num (int)] "
            "-s [slot mask (hex)] -n [num pedestals] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_disc_check,(void *)args);
  return 0; 
}


void *pt_disc_check(void *args)
{
  disc_check_t arg = *(disc_check_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);


  int i,j;
  int errors, slot;
  uint32_t ctemp, count_temp[8][32],count_i[16][32],count_f[16][32],cdiff;
  int chan_errors[16][32];
  int chan_diff[16][32];
  errors = 0;
  uint32_t result;

  pt_printsend("***************************************\n");
  pt_printsend("Starting discriminator check for Crate %d\n",arg.crate_num);

  // set up mtc
  reset_memory();
  set_gt_counter(0);
  //if (setup_pedestals(0,25,150,0,(0x1<<arg.crate_num)+MSK_TUB,(0x1<<arg.crate_num)+MSK_TUB)){
  if (setup_pedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
        (0x1<<arg.crate_num)+MSK_TUB,(0x1<<arg.crate_num)+MSK_TUB)){
    pt_printsend("Error setting up mtc. Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // set up crate
  for (i=0;i<16;i++){
    uint32_t select_reg = FEC_SEL*i;
    uint32_t crate_val = arg.crate_num << FEC_CSR_CRATE_OFFSET;
    xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);
    xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result,arg.crate_num,&thread_fdset);
    xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,crate_val | 0x6,&result,arg.crate_num,&thread_fdset);
    xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,crate_val,&result,arg.crate_num,&thread_fdset);
    xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);
    xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result,arg.crate_num,&thread_fdset);
  }
  deselect_fecs(arg.crate_num,&thread_fdset);

  // get initial data
  uint16_t mask1 = arg.slot_mask & 0xFF;
  uint16_t mask2 = arg.slot_mask & 0xFF00;
  result = get_cmos_total_count(arg.crate_num,mask1,count_temp,&thread_fdset);
  int slot_count = 0;
  for (i=0;i<8;i++)
    if ((0x1<<i) & arg.slot_mask){
      for (j=0;j<32;j++){
        count_i[i][j] = count_temp[slot_count][j];
      }
      slot_count++;
    }
  result = get_cmos_total_count(arg.crate_num,mask2,count_temp,&thread_fdset);
  slot_count = 0;
  for (i=8;i<16;i++)
    if ((0x1<<i) & arg.slot_mask){
      for (j=0;j<32;j++)
        count_i[i][j] = count_temp[slot_count][j];
      slot_count++;
    }


  // let the main loop send pongs back until we need xl3 again
  temp_unlock(0x1<<arg.crate_num);

  // fire pedestals
  i = arg.num_pedestals;
  while (i>0){
    if (i > MAX_PER_PACKET){
      multi_softgt(MAX_PER_PACKET);
      i -= MAX_PER_PACKET;
    }else{
      multi_softgt(i);
      i = 0;
    }
    if (i%50000 == 0)
      pt_printsend("%d\n",i);
  }

  // lock it back
  relock(0x1<<arg.crate_num);

  // get final data
  result = get_cmos_total_count(arg.crate_num,mask1,count_temp,&thread_fdset);
  slot_count = 0;
  for (i=0;i<8;i++)
    if ((0x1<<i) & arg.slot_mask){
      for (j=0;j<32;j++)
        count_f[i][j] = count_temp[slot_count][j];
      slot_count++;
    }
  result = get_cmos_total_count(arg.crate_num,mask2,count_temp,&thread_fdset);
  slot_count = 0;
  for (i=8;i<16;i++)
    if ((0x1<<i) & arg.slot_mask){
      for (j=0;j<32;j++)
        count_f[i][j] = count_temp[slot_count][j];
      slot_count++;
    }

  // flag bad channels
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      for (j=0;j<32;j++){
        chan_errors[i][j] = 0;
        chan_diff[i][j] = 0;
        cdiff = count_f[i][j] - count_i[i][j];
        if (cdiff != arg.num_pedestals){
          pt_printsend("cmos_count != nped for slot %d chan %d. Nped: %d, cdiff: (%d - %d) %d\n",
              i,j,arg.num_pedestals,count_f[i][j],count_i[i][j],cdiff);
          chan_errors[i][j] = 1;
          chan_diff[i][j] = cdiff-arg.num_pedestals;
          errors++;
        }
      }
    }
  }

  pt_printsend("Disc check complete!\n");

  if (arg.update_db){
    pt_printsend("updating the database\n");
    for (slot=0;slot<16;slot++){
      if ((0x1<<slot) & arg.slot_mask){
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("disc_check"));
        int passflag = 1;

        JsonNode *channels = json_mkarray();
        for (i=0;i<32;i++){
          JsonNode *one_chan = json_mkobject();
          json_append_member(one_chan,"id",json_mknumber((double) i));
          json_append_member(one_chan,"count_minus_peds",json_mknumber((double)chan_diff[slot][i]));
          json_append_member(one_chan,"errors",json_mkbool(chan_errors[slot][i]));
          if (chan_errors[slot][i] > 0)
            passflag = 0;
          json_append_element(channels,one_chan);
        }
        json_append_member(newdoc,"channels",channels);

        json_append_member(newdoc,"pass",json_mkbool(passflag));

        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        if (arg.ecal)
          json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
        pt_printsend("lock has been set to %d\n",xl3_lock[arg.crate_num]);
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc); // only need to delete the head node
      }
    }
  }

  pt_printsend("********************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

