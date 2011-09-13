#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"
#include "xl3_registers.h"
#include "mtc_registers.h"
#include "unpack_bundles.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "mtc_utils.h"
#include "fec_utils.h"
#include "daq_utils.h"
#include "fifo_test.h"

int fifo_test(char *buffer)
{
  fifo_test_t *args;
  args = malloc(sizeof(fifo_test_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
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
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: fifo_test -c [crate num (int)] "
            "-s [slot mask (hex)] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_fifo_test,(void *)args);
  return 0; 
}


void *pt_fifo_test(void *args)
{
  fifo_test_t arg = *(fifo_test_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int i,j,k;
  uint32_t result;

  char error_history[10000];
  char cur_msg[1000];
  int slot_errors;
  uint32_t remainder,diff,write,read;
  uint32_t gt1[16],gt2[16],bundle[12];
  uint32_t *readout_data;
  int gtstofire;


  readout_data = (uint32_t *) malloc( 0x000FFFFF * sizeof(uint32_t));
  if (readout_data == NULL){
    pt_printsend("Error mallocing. Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // set up mtc
  reset_memory();
  set_gt_counter(0);
  //FIXME
  //if (setup_pedestals(0,25,150,0,(0x1<<arg.crate_num)+MSK_TUB,(0x1<<arg.crate_num)+MSK_TUB)){
  if (setup_pedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
        (0x1<<arg.crate_num)+MSK_TUB,(0x1<<arg.crate_num)+MSK_TUB)){
    pt_printsend("Error setting up mtc. Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // set up crate
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      uint32_t select_reg = FEC_SEL*i;
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0xf,&result,arg.crate_num,&thread_fdset);
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,arg.crate_num<<FEC_CSR_CRATE_OFFSET,&result,arg.crate_num,&thread_fdset);
    }
  }

  // mask in one channel on each fec
  set_crate_pedestals(arg.crate_num,arg.slot_mask,0x1,&thread_fdset);

  // check initial conditions
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);
      // get initial count
      get_gt_count(&gt1[i]);
    }
  }

  // now pulse the soft gts
  // we will fill the fifos almost to the top
  gtstofire = (0xFFFFF-32)/3;
  pt_printsend("Now firing %u soft gts.\n",gtstofire);
  int gtcount = 0;
  while (gtcount < gtstofire){
    if (gtstofire - gtcount > 5000){
      multi_softgt(5000);
      gtcount += 5000;
    }else{
      multi_softgt(gtstofire-gtcount);
      gtcount += gtstofire-gtcount;
    }
    if (gtcount%15000 == 0){
      pt_printsend(".");
      fflush(stdout);
    }
  }

  pt_printsend("\n");

  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      // zero some stuff
      memset(cur_msg,'\0',1000);
      slot_errors = 0;
      memset(error_history,'\0',10000);

      // get the updated gt count
      get_gt_count(&gt2[i]);
      sprintf(cur_msg,"Slot %d - Number of GTs fired: %u\n",i,gt2[i]-gt1[i]);
      sprintf(cur_msg+strlen(cur_msg),"Slot %d - GT before: %u, after: %u\n",i,gt1[i],gt2[i]);
      pt_printsend("%s",cur_msg);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);

      // make sure it matches the number of gts you sent
      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);
      if (diff != 3*(gt2[i]-gt1[i])){
        sprintf(cur_msg,"Slot %d - Unexpected number of fifo counts!\n",i);
        sprintf(cur_msg+strlen(cur_msg),"Slot %d - Based on MTCD GTs fired, should be 0x%05x (%u)\n",i,3*(gt2[i]-gt1[i]),3*(gt2[i]-gt1[i]));
        sprintf(cur_msg+strlen(cur_msg),"Slot %d - Based on times looped, should be 0x%05x (%u)\n",i,gtstofire*3,gtstofire*3);
        pt_printsend("%s",cur_msg);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
      }

      // turn off all but one slot
      set_crate_pedestals(arg.crate_num,0x1<<i,0x1,&thread_fdset);

      // now pulse the last soft gts to fill fifo to the top
      remainder = diff/3;
      sprintf(cur_msg,"Slot %d - Now firing %d more soft gts\n",i,remainder);
      pt_printsend("%s",cur_msg);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      multi_softgt(remainder);

      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);

      // now read out bundles
      for (j=0;j<12;j++)
        xl3_rw(READ_MEM + FEC_SEL*i,0x0,&bundle[j],arg.crate_num,&thread_fdset);

      sprintf(cur_msg,"Slot %d - Read out %d longwords (%d bundles)\n",i,12,12/3);
      pt_printsend("%s",cur_msg);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);

      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);
      remainder = diff/3;
      dump_pmt_verbose(12/3,bundle,error_history);

      // check overflow behavior
      sprintf(cur_msg,"Slot %d - Now overfill FEC (firing %d more soft GTs)\n",i,remainder+3);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);
      multi_softgt(remainder+3);

      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);
      uint32_t busy_bits,test_id;
      xl3_rw(CMOS_BUSY_BIT(0) + READ_REG + FEC_SEL*i,0x0,&busy_bits,arg.crate_num,&thread_fdset);
      sprintf(cur_msg,"Should see %d cmos busy bits set. Busy bits are -> 0x%04x\n",3,busy_bits & 0x0000FFFF);
      sprintf(cur_msg+strlen(cur_msg),"(Note that there might be one less than expected as it might be caught up in sequencing.)\n");

      xl3_rw(CMOS_INTERN_TEST(0) + READ_REG + FEC_SEL*i,0x0,&test_id,arg.crate_num,&thread_fdset);
      sprintf(cur_msg+strlen(cur_msg),"See if we can read out test reg: 0x%08x\n",test_id);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      // now read out bundles
      for (j=0;j<12;j++)
        xl3_rw(READ_MEM + FEC_SEL*i,0x0,&bundle[j],arg.crate_num,&thread_fdset);
      
      sprintf(cur_msg,"Slot %d - Read out %d longwords (%d bundles). Should have cleared all busy bits\n",i,12,12/3);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      dump_pmt_verbose(12/3,bundle,error_history);
      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);

      xl3_rw(CMOS_BUSY_BIT(0) + READ_REG + FEC_SEL*i,0x0,&busy_bits,arg.crate_num,&thread_fdset);
      sprintf(cur_msg,"Should see %d cmos busy bits set. Busy bits are -> 0x%04x\n",0,busy_bits & 0x0000FFFF);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      // read out data and check the stuff around the wrap of the write pointer
      j = 30;
      sprintf(cur_msg,"Slot %d - Dumping all but the last %d events.\n",i,j);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);
      int count = read_out_bundles(arg.crate_num,i,(0xFFFFF-diff)/3-j,readout_data,&thread_fdset);
      printf("Managed to read out %d bundles\n",count);

      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);
      j = (0x000FFFFF-diff)/3;

      sprintf(cur_msg,"Slot %d - Dumping last %d events!\n",i,j);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      if (j > 0xFFFFF/3){
        pt_printsend("There was an error calculating how much to read out. Will attempt to read everything thats left\n");
        j = 0xFFFFF/3;
      }
      read_out_bundles(arg.crate_num, i, j, readout_data,&thread_fdset);
      dump_pmt_verbose(j, readout_data,error_history);
      check_fifo(&diff,arg.crate_num,i,error_history,&thread_fdset);

      sprintf(cur_msg,"Slot %d - Trying to read past the end... should get %d bus errors\n",i,12);
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      int busstop = 0;
      for (j=0;j<12;j++)
        busstop += xl3_rw(READ_MEM + FEC_SEL*i,0x0,&bundle[j],arg.crate_num,&thread_fdset);
      if (busstop){
        sprintf(cur_msg,"Slot %d - Got expected bus errors (%d).\n",i,busstop);
      }else{
        sprintf(cur_msg,"Slot %d - Error! Read past end!\n",i);
        slot_errors = 1;
      }
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      deselect_fecs(arg.crate_num,&thread_fdset);

      sprintf(cur_msg,"Finished Slot %d\n",i);
      sprintf(cur_msg+strlen(cur_msg),"**************************************************\n");
      sprintf(error_history+strlen(error_history),"%s",cur_msg);
      pt_printsend("%s",cur_msg);

      if (arg.update_db){
        pt_printsend("updating the database\n");
        printsend("updating slot %d\n",i);
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("fifo_test"));
        json_append_member(newdoc,"printout",json_mkstring(error_history));
        json_append_member(newdoc,"pass",json_mkbool(~(slot_errors)));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	
        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
        json_delete(newdoc);
      }

    } // end if slot mask
  } // end loop over slot

  pt_printsend("Ending fifo test\n");
  pt_printsend("********************************\n");


  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

static void check_fifo(uint32_t *thediff, int crate_num, int slot_num, char *msg_buff, fd_set *thread_fdset)
{
  uint32_t diff,read,write,select_reg;
  float remainder;
  char msg[5000];
  select_reg = FEC_SEL*slot_num;
  xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&diff,crate_num,thread_fdset);
  xl3_rw(FIFO_READ_PTR_R + select_reg + READ_REG,0x0,&read,crate_num,thread_fdset);
  xl3_rw(FIFO_WRITE_PTR_R + select_reg + READ_REG,0x0,&write,crate_num,thread_fdset);
  diff &= 0x000FFFFF;
  read &= 0x000FFFFF;
  write &= 0x000FFFFF;
  remainder = (float) 0x000FFFFF - (float) diff;

  sprintf(msg,"**************************************\n");
  sprintf(msg+strlen(msg),"Fifo diff ptr is %05x\n",diff);
  sprintf(msg+strlen(msg),"Fifo write ptr is %05x\n",write);
  sprintf(msg+strlen(msg),"Fifo read ptr is %05x\n",read);
  sprintf(msg+strlen(msg),"Left over space is %2.1f (%2.1f bundles)\n",remainder,remainder/3.0);
  sprintf(msg+strlen(msg),"Total events in memory is %2.1f.\n",(float) diff / 3.0);
  sprintf(msg_buff+strlen(msg_buff),"%s",msg);
  pt_printsend("%s",msg);
  *thediff = (uint32_t) remainder;
}
