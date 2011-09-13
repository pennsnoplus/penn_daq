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
#include "mb_stability_test.h"

int mb_stability_test(char *buffer)
{
  mb_stability_test_t *args;
  args = malloc(sizeof(mb_stability_test_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->num_pedestals = 1000;
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
      }else if (words[1] == 'n'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->num_pedestals = atoi(words2);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: mb_stability_test -c [crate num (int)] "
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
  pthread_create(new_thread,NULL,pt_mb_stability_test,(void *)args);
  return 0; 
}


void *pt_mb_stability_test(void *args)
{
  mb_stability_test_t arg = *(mb_stability_test_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int i,j,k;
  uint32_t result;

  char error_history[16][5000];
  char temp_msg[1000];
  int slot_errors[16];
  uint32_t chan_mask_rand[4],pmtword[3];
  uint32_t crate,slot,chan,nc_cc,gt8,gt16,gtword,cmos_es16,cgt_es16,cgt_es8;
  uint32_t fec_diff,nfire,nfire_16,nfire_24,nfire_gtid;
  int num_chan, rd, num_print;

  // zero some stuff
  memset(temp_msg,'\0',1000);
  for (i=0;i<16;i++){
    slot_errors[i] = 0;
    memset(error_history[i],'\0',5000);
  }
  rd = 0;
  num_chan = 8;
  nfire_16 = 0;
  nfire_24 = 0;
  num_print = 10;
  chan_mask_rand[0] = 0x11111111;
  chan_mask_rand[1] = 0x22222222;
  chan_mask_rand[2] = 0x44444444;
  chan_mask_rand[3] = 0x88888888;

  pt_printsend("*** Starting MB+DB stability test ***\n");
  pt_printsend("Crate: %d, slot mask: %04x\n",arg.crate_num,arg.slot_mask);

  pt_printsend("Channel mask used: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
      chan_mask_rand[0],chan_mask_rand[1],chan_mask_rand[2],chan_mask_rand[3]);
  pt_printsend("Going to fire %d times\n",arg.num_pedestals);
  for (i=0;i<16;i++)
    sprintf(error_history[i]+strlen(error_history[i]),"Going to fire %d times\n",arg.num_pedestals);

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


  for (nfire=1;nfire<arg.num_pedestals;nfire++){
    nfire_16++;
    if (nfire_16 == 65535){
      nfire_16 = 0;
      nfire_24++;
    }
    nfire_gtid = nfire_24*0x10000 + nfire_16;

    // for selected slots, set semi-random pattern
    for (i=0;i<16;i++){
      if (((0x1<<i)& arg.slot_mask) && (slot_errors[i] == 0)){
        uint32_t select_reg = FEC_SEL*i;
        //enable pedestals
        xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,chan_mask_rand[rd],&result,arg.crate_num,&thread_fdset);
      }
    }
    deselect_fecs(arg.crate_num,&thread_fdset);
    rd = (rd+1)%4;

    // fire pulser once
    if (nfire == num_print){
      pt_printsend("Pulser fired %u times.\n",nfire);
      for (i=0;i<16;i++){
        if (((0x1<<i) & arg.slot_mask) && (slot_errors[i] == 0)){
          sprintf(error_history[i]+strlen(error_history[i]),"Pulser fired %u times.\n",nfire);
        }
      }
      num_print+=10;
    }
    usleep(1);
    send_softgt();
    usleep(1);

    for (j=0;j<16;j++){
      if (((0x1<<j) & arg.slot_mask) && (slot_errors[j] == 0)){
        uint32_t select_reg = FEC_SEL*j;

        // check fifo diff pointer
        xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&fec_diff,arg.crate_num,&thread_fdset);
        fec_diff &= 0x000FFFFF;
        if (fec_diff != num_chan*3){
          sprintf(temp_msg,">>>Error, fec_diff = %d, expected %d\n",fec_diff,num_chan*3);
          sprintf(temp_msg+strlen(temp_msg),">>>testing crate %d, slot %d\n",arg.crate_num,j);
          sprintf(temp_msg+strlen(temp_msg),">>>stopping at pulser iteration %u\n",nfire);
          sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
          pt_printsend("%s",temp_msg);
          slot_errors[j] = 1 ;
        }
      }
    }

    for (j=0;j<16;j++){
      if (((0x1<<j) & arg.slot_mask) && (slot_errors[j] == 0)){
        uint32_t select_reg = FEC_SEL*j;

        // readout loop, check fifo again while reading out
        int iter = 0;
        while(3 <= fec_diff){
          iter++;
          if (iter > num_chan*3){
            sprintf(temp_msg,">>>Error, number of FEC reads exceeds %d, aborting!\n",num_chan*3);
            sprintf(temp_msg+strlen(temp_msg),">>>testing crate %d, slot %d\n",arg.crate_num,j);
            sprintf(temp_msg+strlen(temp_msg),">>>stopping at pulser iteration %u\n",nfire);
            sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
            pt_printsend("%s",temp_msg);
            slot_errors[j] = 1 ;
            break;
          }

          //read out memory
          xl3_rw(READ_MEM + select_reg,0x0,pmtword,arg.crate_num,&thread_fdset);
          xl3_rw(READ_MEM + select_reg,0x0,pmtword+1,arg.crate_num,&thread_fdset);
          xl3_rw(READ_MEM + select_reg,0x0,pmtword+2,arg.crate_num,&thread_fdset);
          xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&fec_diff,arg.crate_num,&thread_fdset);
          fec_diff &= 0x000FFFFF;

          crate = (uint32_t) UNPK_CRATE_ID(pmtword);
          slot = (uint32_t) UNPK_BOARD_ID(pmtword);
          chan = (uint32_t) UNPK_CHANNEL_ID(pmtword);
          gt8 = (uint32_t) UNPK_FEC_GT8_ID(pmtword);
          gt16 = (uint32_t) UNPK_FEC_GT16_ID(pmtword);
          cmos_es16 = (uint32_t) UNPK_CMOS_ES_16(pmtword);
          cgt_es16 = (uint32_t) UNPK_CGT_ES_16(pmtword);
          cgt_es8 = (uint32_t) UNPK_CGT_ES_24(pmtword);
          nc_cc = (uint32_t) UNPK_NC_CC(pmtword);

          // check crate, slot, nc_cc
          if ((crate != arg.crate_num) || (slot != j) || (nc_cc != 0)){
            sprintf(temp_msg,"***************************************\n");
            sprintf(temp_msg+strlen(temp_msg),"Crate/slot or Nc_cc error. Pedestal iter: %u\n",nfire);
            sprintf(temp_msg+strlen(temp_msg),"Expected crate,slot,nc_cc: %d %d %d\n",arg.crate_num,j,0);
            sprintf(temp_msg+strlen(temp_msg),"Found crate,slot,chan,nc_cc: %d %d %d %d\n",crate,slot,chan,nc_cc);
            sprintf(temp_msg+strlen(temp_msg),"Bundle 0,1,2: 0x%08x 0x%08x 0x%08x\n",pmtword[0],pmtword[1],pmtword[2]);
            sprintf(temp_msg+strlen(temp_msg),"***************************************\n");
            sprintf(temp_msg+strlen(temp_msg),">>>Stopping at pulser iteration %u\n",nfire);
            sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
            pt_printsend("%s",temp_msg);
            slot_errors[j] = 1;
            break;
          }

          // check gt increment
          gtword = gt8*0x10000 + gt16;
          if (gtword != nfire_gtid){
            sprintf(temp_msg,"***************************************\n");
            sprintf(temp_msg+strlen(temp_msg),"GT8/16 error, expect GTID: %u \n",nfire_gtid);
            sprintf(temp_msg+strlen(temp_msg),"Crate,slot,chan: %d %d %d\n",crate,slot,chan);
            sprintf(temp_msg+strlen(temp_msg),"Bundle 0,1,2: 0x%08x 0x%08x 0x%08x\n",pmtword[0],pmtword[1],pmtword[2]);
            sprintf(temp_msg+strlen(temp_msg),"Found gt8, gt16, gtword: %d %d %08x\n",gt8,gt16,gtword);
            sprintf(temp_msg+strlen(temp_msg),"***************************************\n");
            sprintf(temp_msg+strlen(temp_msg),">>>Stopping at pulser iteration %u\n",nfire);
            sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
            pt_printsend("%s",temp_msg);
            slot_errors[j] = 1;
            break;
          }


          // check synclear bits
          if ((cmos_es16 == 1) || (cgt_es16 == 1) || (cgt_es8 == 1)){
            if (gt8 != 0) {
              sprintf(temp_msg,"***************************************\n");
              sprintf(temp_msg+strlen(temp_msg),"Synclear error, GTID: %u\n",nfire_gtid);
              sprintf(temp_msg+strlen(temp_msg),"crate, slot, chan: %d %d %d\n",crate,slot,chan);
              sprintf(temp_msg+strlen(temp_msg),"Bundle 0,1,2: 0x%08x 0x%08x 0x%08x\n",pmtword[0],pmtword[1],pmtword[2]);
              sprintf(temp_msg+strlen(temp_msg),"Found cmos_es16,cgt_es16,cgt_es8: %d %d %d\n",cmos_es16,cgt_es16,cgt_es8);
              sprintf(temp_msg+strlen(temp_msg),"***************************************\n");
              sprintf(temp_msg+strlen(temp_msg),">>>Stopping at pulser iteration %u\n",nfire);
              sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
              pt_printsend("%s",temp_msg);
              slot_errors[j] = 1;
              break;
            }
          }

        } // while reading out

        deselect_fecs(arg.crate_num,&thread_fdset);

      } //end if slot mask
    } // end loop over slots
  } // loop over nfire

  if (arg.update_db){
    pt_printsend("updating the database\n");
    int slot;
    for (slot=0;slot<16;slot++){
      if ((0x1<<slot) & arg.slot_mask){
        pt_printsend("updating slot %d\n",slot);
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("mb_stability_test"));
        json_append_member(newdoc,"printout",json_mkstring(error_history[slot]));
        json_append_member(newdoc,"pass",json_mkbool(!(slot_errors[slot])));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc);
      }
    }
  }


  pt_printsend("Ending mb stability test\n");
  pt_printsend("********************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

