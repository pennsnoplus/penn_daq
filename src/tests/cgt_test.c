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
#include "cgt_test.h"

int cgt_test(char *buffer)
{
  cgt_test_t *args;
  args = malloc(sizeof(cgt_test_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->pattern = 0xFFFFFFFF;
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
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->pattern = strtoul(words2,(char**)NULL,16);
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
        printsend("Usage: cgt_test -c [crate num (int)] "
            "-s [slot mask (hex)] -p [channel_mask (hex)] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_cgt_test,(void *)args);
  return 0; 
}


void *pt_cgt_test(void *args)
{
  cgt_test_t arg = *(cgt_test_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int i,j,k;
  uint32_t result;

  uint32_t bundles[3];
  uint32_t crate_id,slot_id,chan_id,nc_id,gt16_id,gt8_id,es16;

  int missing_bundles[16],chan_errors[16][32];
  char error_history[16][5000];
  char cur_msg[1000];
  int max_errors[16];
  uint32_t badchanmask;
  int num_chans;

  // zero some stuff
  memset(cur_msg,'\0',1000);
  num_chans = 0;
  for (i=0;i<32;i++)
    if ((0x1<<i) & arg.pattern)
      num_chans++;
  for (i=0;i<16;i++){
    missing_bundles[i] = 0;
    max_errors[i] = 0;
    memset(error_history[i],'\0',5000);
  }

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
  }
  deselect_fecs(arg.crate_num,&thread_fdset);

  pt_printsend("Starting GT increment test\n"
      "Crate number: %d\n"
      "Slot and Channel mask: %08x %08x\n",arg.crate_num,arg.slot_mask,arg.pattern);

  // select desired fecs
  if (set_crate_pedestals(arg.crate_num,arg.slot_mask,arg.pattern,&thread_fdset)){
    pt_printsend("Error setting up crate for pedestals. Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }
  deselect_fecs(arg.crate_num,&thread_fdset);

  uint32_t num_peds = 0xFFFF + 10000;
  pt_printsend("Going to fire pulser %u times.\n",num_peds);

  XL3_Packet packet;
  int total_pulses = 0;
  int numgt = 0;
  // we now send out gts in bunches, checking periodically
  // that we are getting the right count at the fecs
  for (j=0;j<16;j++){
    // we skip 4999 gtids then check each 5000th one
    if (j != 13)
      numgt = 4999;
    else
      numgt = 534;

    multi_softgt(numgt);

    // now loop over slots and make sure we got all the gts
    for (i=0;i<16;i++){
      if (((0x1<<i) & arg.slot_mask) && (max_errors[i] == 0)){
        uint32_t select_reg = FEC_SEL*i;
        xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,
            &result,arg.crate_num,&thread_fdset);
        if ((result & 0x000FFFFF) != numgt*3*num_chans){
          sprintf(cur_msg,"Not enough bundles slot %d: expected %d, found %u\n",
              i,numgt*3*num_chans,result & 0x000FFFFF);
          pt_printsend("%s",cur_msg);
          sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
          missing_bundles[i] = 1;
        }

        // reset the fifo
        packet.cmdHeader.packet_type = RESET_FIFOS_ID;
        reset_fifos_args_t *packet_args = (reset_fifos_args_t *) packet.payload;
        packet_args->slot_mask = arg.slot_mask;
        SwapLongBlock(packet_args,sizeof(reset_fifos_args_t)/sizeof(uint32_t));
        do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
        xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,
            (arg.crate_num << FEC_CSR_CRATE_OFFSET),&result,arg.crate_num,&thread_fdset);
      } // end if in slot mask and not max errors
    } // end loop over slots

    // now send a single soft gt and make sure it looks good
    send_softgt();

    total_pulses += numgt+1;
    if (j == 13)
      total_pulses++; // rollover bug

    for (i=0;i<16;i++){
      if (((0x1<<i) & arg.slot_mask) && (max_errors[i] == 0)){
        uint32_t select_reg = FEC_SEL*i;
        xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,
            &result,arg.crate_num,&thread_fdset);
        if ((result & 0x000FFFFF) != 3*num_chans){
          sprintf(cur_msg,"Not enough bundles slot %d: expected %d, found %u\n",
              i,3*num_chans,result & 0x000FFFFF);
          pt_printsend("%s",cur_msg);
          sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
          missing_bundles[i] = 1;
        }

        // read out one bundle for each channel
        badchanmask = arg.pattern;
        for (k=0;k<((result&0x000FFFFF)/3);k++){
          xl3_rw(READ_MEM + select_reg,0x0,&bundles[0],arg.crate_num,&thread_fdset);
          xl3_rw(READ_MEM + select_reg,0x0,&bundles[1],arg.crate_num,&thread_fdset);
          xl3_rw(READ_MEM + select_reg,0x0,&bundles[2],arg.crate_num,&thread_fdset);

          crate_id = (int) UNPK_CRATE_ID(bundles); 
          slot_id = (int) UNPK_BOARD_ID(bundles);
          chan_id = (int) UNPK_CHANNEL_ID(bundles);
          nc_id = (int) UNPK_NC_CC(bundles);
          gt16_id = (int) UNPK_FEC_GT16_ID(bundles);
          gt8_id = (int) UNPK_FEC_GT8_ID(bundles);
          es16 = (int) UNPK_CGT_ES_16(bundles);

          badchanmask &= ~(0x1<<chan_id);

          if (crate_id != arg.crate_num){
            sprintf(cur_msg,"Crate wrong for slot %d, chan %u: expected %d, read %u\n",
                i,chan_id,arg.crate_num,crate_id);
            pt_printsend("%s",cur_msg);
            sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            chan_errors[i][chan_id] = 1;
          } 
          if (slot_id != i){
            sprintf(cur_msg,"Slot wrong for slot %d chan %u: expected %d, read %u\n",
                i,chan_id,i,chan_id);
            pt_printsend("%s",cur_msg);
            sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            chan_errors[i][chan_id] = 1;
          } 
          if (nc_id != 0x0){
            sprintf(cur_msg,"NC_CC wrong for slot %d chan %u: expected %d, read %u\n",
                i,chan_id,0,nc_id);
            pt_printsend("%s",cur_msg);
            sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            chan_errors[i][chan_id] = 1;
          } 
          if ((gt16_id + (65536*gt8_id)) != total_pulses){
            if (gt16_id == total_pulses%65536){
              sprintf(cur_msg,"Bad upper 8 Gtid bits for slot %d chan %u: expected %d, read %u\n"
                  "%08x %08x %08x\n",
                  i,chan_id,total_pulses-total_pulses%65536,(65536*gt8_id),bundles[0],
                  bundles[1],bundles[2]);
              pt_printsend("%s",cur_msg);
              sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);

            }else if (gt8_id == total_pulses/65536){
              sprintf(cur_msg,"Bad lower 16 gtid bits for slot %d chan %u: expected %d, read %u\n"
                  "%08x %08x %08x\n",
                  i,chan_id,total_pulses%65536,gt16_id,bundles[0],
                  bundles[1],bundles[2]);
              pt_printsend("%s",cur_msg);
              sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            }else{
              sprintf(cur_msg,"Bad gtid for slot %d chan %u: expected %d, read %u\n"
                  "%08x %08x %08x\n",
                  i,chan_id,total_pulses,gt16_id+(65536*gt8_id),bundles[0],
                  bundles[1],bundles[2]);
              pt_printsend("%s",cur_msg);
              sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            }
            chan_errors[i][chan_id] = 1;
          } 
          if (es16 != 0x0 && j >= 13){
            sprintf(cur_msg,"Synclear error for slot %d chan %u.\n",
                i,chan_id);
            pt_printsend("%s",cur_msg);
            sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            chan_errors[i][chan_id] = 1;
          } 
        } // end loop over bundles being read out

        for (k=0;k<32;k++){
          if ((0x1<<k) & badchanmask){
            sprintf(cur_msg,"No bundle found for slot %d chan %d\n",i,k);
            pt_printsend("%s",cur_msg);
            sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            chan_errors[i][k] = 1;
          }
        }

      } // end if in slot mask and not max errors
    } // end loop over slots

    // check if we should stop any slot
    // because there are too many errors
    for (i=0;i<16;i++){
      if ((strlen(error_history[i]) > 5000) && (max_errors[i] == 0)){
        pt_printsend("Too many errors slot %d. Skipping that slot\n",i);
        max_errors[i] = 1;
      }
    }

    pt_printsend("%d pulses\n",total_pulses);
    for (i=0;i<16;i++)
      sprintf(error_history[i]+strlen(error_history[i]),"%d pulses\n",total_pulses);

  } // end loop over gt bunches

  if (arg.update_db){
    pt_printsend("updating the database\n");
    int slot,passflag;
    for (slot=0;slot<16;slot++){
      if ((0x1<<slot) & arg.slot_mask){
        pt_printsend("updating slot %d\n",slot);
        passflag = 1;
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("cgt_test"));
        json_append_member(newdoc,"missing_bundles",json_mkbool(missing_bundles[slot]));
        if (missing_bundles[slot] > 0)
          passflag = 0;
        JsonNode *chan_errs = json_mkarray();
        for (i=0;i<32;i++){
          json_append_element(chan_errs,json_mkbool(chan_errors[slot][i]));
          if (chan_errors[slot][i] > 0)
            passflag = 0;
        }
        json_append_member(newdoc,"errors",chan_errs);
        json_append_member(newdoc,"printout",json_mkstring(error_history[slot]));
        json_append_member(newdoc,"pass",json_mkbool(passflag));

        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        if (arg.ecal)
          json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc); // only delete the head node
      }
    }
  }

  pt_printsend("Ending cgt test\n");
  pt_printsend("********************************\n");


  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

