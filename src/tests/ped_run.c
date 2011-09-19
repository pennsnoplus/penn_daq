#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"
#include "xl3_registers.h"
#include "unpack_bundles.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "mtc_utils.h"
#include "fec_utils.h"
#include "xl3_utils.h"
#include "daq_utils.h"
#include "ped_run.h"

int ped_run(char *buffer)
{
  ped_run_t *args;
  args = malloc(sizeof(ped_run_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->pattern = 0xFFFFFFFF;
  args->num_pedestals = 50;
  args->ped_low = 400;
  args->ped_high = 700;
  args->frequency = 0;
  args->gt_delay = DEFAULT_GT_DELAY;
  args->ped_width = DEFAULT_PED_WIDTH;
  args->balanced = 0;
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
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->pattern = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'n'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->num_pedestals = atoi(words2);
      }else if (words[1] == 'l'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->ped_low = atoi(words2);
      }else if (words[1] == 'u'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->ped_high = atoi(words2);
      }else if (words[1] == 'f'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->frequency = atof(words2);
      }else if (words[1] == 't'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->gt_delay = atoi(words2);
      }else if (words[1] == 'w'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->ped_width = atoi(words2);
      }else if (words[1] == 'b'){args->balanced = 1;
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: ped_run -c [crate num (int)] "
            "-s [slot mask (hex)] -p [channel mask (hex)] "
            "-l [lower Q ped check value] -u [upper Q ped check value] "
            "-f [pulser frequency (0 for softgts)] -n [number of pedestals per cell] "
            "-t [gt delay] -w [pedestal width] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_ped_run,(void *)args);
  return 0; 
}


void *pt_ped_run(void *args)
{
  ped_run_t arg = *(ped_run_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  pt_printsend("Pedestal Run Setup\n");
  pt_printsend("-------------------------------------------\n");
  pt_printsend("Crate:		    %2d\n",arg.crate_num);
  pt_printsend("Slot Mask:		    0x%4hx\n",arg.slot_mask);
  pt_printsend("Pedestal Mask:	    0x%08lx\n",arg.pattern);
  pt_printsend("GT delay (ns):	    %3hu\n", arg.gt_delay);
  pt_printsend("Pedestal Width (ns):    %2d\n",arg.ped_width);
  pt_printsend("Pulser Frequency (Hz):  %3.0f\n",arg.frequency);
  pt_printsend("Lower/Upper pedestal check range: %d %d\n",arg.ped_low,arg.ped_high);

  uint32_t *pmt_buffer = malloc(0x100000*sizeof(uint32_t));
  struct pedestal *ped = malloc(32*sizeof(struct pedestal)); 

  int i,j;
  int num_channels = 0;
  for (i=0;i<32;i++)
    if ((0x1<<i) & arg.pattern)
      num_channels++;
  // set up crate
  change_mode(INIT_MODE,0x0,arg.crate_num,&thread_fdset);
  int slot;
  for (slot=0;slot<16;slot++){
    if ((0x1<<slot) & arg.slot_mask){
      uint32_t select_reg = FEC_SEL*slot;
      uint32_t result;
      xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result,arg.crate_num,&thread_fdset);
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result,arg.crate_num,&thread_fdset);
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);
      xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);
    }
  }
  deselect_fecs(arg.crate_num,&thread_fdset);

  int errors = fec_load_crate_addr(arg.crate_num,arg.slot_mask,&thread_fdset);
  errors += set_crate_pedestals(arg.crate_num,arg.slot_mask,arg.pattern,&thread_fdset);
  deselect_fecs(arg.crate_num,&thread_fdset);
  if (errors){
    pt_printsend("Error setting up crate for pedestals. Exiting\n");
    free(pmt_buffer);
    free(ped);
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // set up MTC
  errors = setup_pedestals(arg.frequency,arg.ped_width,arg.gt_delay,DEFAULT_GT_FINE_DELAY,
      (0x1<<arg.crate_num),(0x1<<arg.crate_num));
  if (errors){
    pt_printsend("Error setting up MTC for pedestals. Exiting\n");
    unset_ped_crate_mask(MASKALL);
    unset_gt_crate_mask(MASKALL);
    free(pmt_buffer);
    free(ped);
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // send pedestals
  if (arg.frequency == 0){
    multi_softgt(arg.num_pedestals*16);
    disable_pulser();
  }else{
    float wait_time = (float) arg.num_pedestals*16.0/arg.frequency*1E6;
    usleep((int) wait_time);
    disable_pulser();
  }


  // loop over slots
  errors = 0;
  for (slot=0;slot<16;slot++){
    if ((0x1<<slot) & arg.slot_mask){

      // initialize pedestal struct
      for (i=0;i<32;i++){
        ped[i].channelnumber = i;
        ped[i].per_channel = 0;
        for (j=0;j<16;j++){
          ped[i].thiscell[j].cellno = j;
          ped[i].thiscell[j].per_cell = 0;
          ped[i].thiscell[j].qlxbar = 0;
          ped[i].thiscell[j].qlxrms = 0;
          ped[i].thiscell[j].qhlbar = 0;
          ped[i].thiscell[j].qhlrms = 0;
          ped[i].thiscell[j].qhsbar = 0;
          ped[i].thiscell[j].qhsrms = 0;
          ped[i].thiscell[j].tacbar = 0;
          ped[i].thiscell[j].tacrms = 0;
        }
      }

      // readout bundles
      int count = read_out_bundles(arg.crate_num,slot,arg.num_pedestals*16*num_channels,
          1,pmt_buffer,&thread_fdset);

      if (count <= 0){
        pt_printsend("There was an error in the count!\n");
        pt_printsend("Errors reading out MB %2d (errno %d)\n",slot,count);
        errors++;
        continue;
      }

      pt_printsend("MB %d: %d bundles read out.\n",slot,count);
      if (count < arg.num_pedestals*16*num_channels)
        errors++;

      // process data
      uint32_t *pmt_iter = pmt_buffer;
      int ch,cell,crateID,num_events;

      for (i=0;i<count;i++){
        crateID = (int) UNPK_CRATE_ID(pmt_iter);
        if (crateID != arg.crate_num){
          printsend( "Invalid crate ID seen! (crate ID %2d, bundle %2i)\n",crateID,i);

          pmt_iter+=3;
          continue;
        }
        ch = (int) UNPK_CHANNEL_ID(pmt_iter);
        cell = (int) UNPK_CELL_ID(pmt_iter);
        ped[ch].thiscell[cell].qlxbar += (double) MY_UNPK_QLX(pmt_iter);
        ped[ch].thiscell[cell].qhsbar += (double) UNPK_QHS(pmt_iter);
        ped[ch].thiscell[cell].qhlbar += (double) UNPK_QHL(pmt_iter);
        ped[ch].thiscell[cell].tacbar += (double) UNPK_TAC(pmt_iter);

        ped[ch].thiscell[cell].qlxrms += pow((double) MY_UNPK_QLX(pmt_iter),2.0);
        ped[ch].thiscell[cell].qhsrms += pow((double) UNPK_QHS(pmt_iter),2.0);
        ped[ch].thiscell[cell].qhlrms += pow((double) UNPK_QHL(pmt_iter),2.0);
        ped[ch].thiscell[cell].tacrms += pow((double) UNPK_TAC(pmt_iter),2.0);

        ped[ch].per_channel++;
        ped[ch].thiscell[cell].per_cell++;

        pmt_iter += 3; //increment pointer
      }

      // do final step
      // final step of calculation
      for (i=0;i<32;i++){
        if (ped[i].per_channel > 0){
          for (j=0;j<16;j++){
            num_events = ped[i].thiscell[j].per_cell;

            //don't do anything if there is no data here or n=1 since
            //that gives 1/0 below.
            if (num_events > 1){

              // now x_avg = sum(x) / N, so now xxx_bar is calculated
              ped[i].thiscell[j].qlxbar /= num_events;
              ped[i].thiscell[j].qhsbar /= num_events;
              ped[i].thiscell[j].qhlbar /= num_events;
              ped[i].thiscell[j].tacbar /= num_events;

              // now x_rms^2 = n/(n-1) * (<xxx^2>*N/N - xxx_bar^2)
              ped[i].thiscell[j].qlxrms = num_events / (num_events -1)
                * ( ped[i].thiscell[j].qlxrms / num_events
                    - pow( ped[i].thiscell[j].qlxbar, 2.0));
              ped[i].thiscell[j].qhlrms = num_events / (num_events -1)
                * ( ped[i].thiscell[j].qhlrms / num_events
                    - pow( ped[i].thiscell[j].qhlbar, 2.0));
              ped[i].thiscell[j].qhsrms = num_events / (num_events -1)
                * ( ped[i].thiscell[j].qhsrms / num_events
                    - pow( ped[i].thiscell[j].qhsbar, 2.0));
              ped[i].thiscell[j].tacrms = num_events / (num_events -1)
                * ( ped[i].thiscell[j].tacrms / num_events
                    - pow( ped[i].thiscell[j].tacbar, 2.0));

              // finally x_rms = sqrt(x_rms^2)
              ped[i].thiscell[j].qlxrms = sqrt(ped[i].thiscell[j].qlxrms);
              ped[i].thiscell[j].qhsrms = sqrt(ped[i].thiscell[j].qhsrms);
              ped[i].thiscell[j].qhlrms = sqrt(ped[i].thiscell[j].qhlrms);
              ped[i].thiscell[j].tacrms = sqrt(ped[i].thiscell[j].tacrms);
            }else{
              ped[i].thiscell[j].qlxrms = 0;
              ped[i].thiscell[j].qhsrms = 0;
              ped[i].thiscell[j].qhlrms = 0;
              ped[i].thiscell[j].tacrms = 0;
            }
          }
        }
      }

      // print results
      pt_printsend("########################################################\n");
      pt_printsend("Slot (%2d)\n", slot);
      pt_printsend("########################################################\n");
      
      uint32_t error_flag[32];

      for (i = 0; i<32; i++){
        error_flag[i] = 0;
        if ((0x1<<i) & arg.pattern){
          pt_printsend("Ch Cell  #   Qhl         Qhs         Qlx         TAC\n");
          for (j=0;j<16;j++){
            pt_printsend("%2d %3d %4d %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f\n",
                i,j,ped[i].thiscell[j].per_cell,
                ped[i].thiscell[j].qhlbar, ped[i].thiscell[j].qhlrms,
                ped[i].thiscell[j].qhsbar, ped[i].thiscell[j].qhsrms,
                ped[i].thiscell[j].qlxbar, ped[i].thiscell[j].qlxrms,
                ped[i].thiscell[j].tacbar, ped[i].thiscell[j].tacrms);
            if (ped[i].thiscell[j].per_cell < arg.num_pedestals*.8 || ped[i].thiscell[j].per_cell > arg.num_pedestals*1.2)
              error_flag[i] |= 0x1;
            if (ped[i].thiscell[j].qhlbar < arg.ped_low || 
                ped[i].thiscell[j].qhlbar > arg.ped_high ||
                ped[i].thiscell[j].qhsbar < arg.ped_low ||
                ped[i].thiscell[j].qhsbar > arg.ped_high ||
                ped[i].thiscell[j].qlxbar < arg.ped_low ||
                ped[i].thiscell[j].qlxbar > arg.ped_high)
              error_flag[i] |= 0x2;
            //printsend("%d %d %d %d\n",ped[i].thiscell[j].qhlbar,ped[i].thiscell[j].qhsbar,ped[i].thiscell[j].qlxbar,ped[i].thiscell[j].tacbar);
          }
          if (error_flag[i] & 0x1)
            pt_printsend(">>>Bad Q pedestal for this channel\n");
          if (error_flag[i] & 0x2)
            printsend(">>>Wrong no of pedestals for this channel\n");
        }
      }


      // update database
      if (arg.update_db){
        printsend("updating the database\n");
        JsonNode *newdoc = json_mkobject();
        JsonNode *num = json_mkarray();
        JsonNode *qhl = json_mkarray();
        JsonNode *qhlrms = json_mkarray();
        JsonNode *qhs = json_mkarray();
        JsonNode *qhsrms = json_mkarray();
        JsonNode *qlx = json_mkarray();
        JsonNode *qlxrms = json_mkarray();
        JsonNode *tac = json_mkarray();
        JsonNode *tacrms = json_mkarray();
        JsonNode *error_node = json_mkarray();
        JsonNode *error_flags = json_mkarray();
        for (i=0;i<32;i++){
          JsonNode *numtemp = json_mkarray();
          JsonNode *qhltemp = json_mkarray();
          JsonNode *qhlrmstemp = json_mkarray();
          JsonNode *qhstemp = json_mkarray();
          JsonNode *qhsrmstemp = json_mkarray();
          JsonNode *qlxtemp = json_mkarray();
          JsonNode *qlxrmstemp = json_mkarray();
          JsonNode *tactemp = json_mkarray();
          JsonNode *tacrmstemp = json_mkarray();
          for (j=0;j<16;j++){
            json_append_element(numtemp,json_mknumber(ped[i].thiscell[j].per_cell));
            json_append_element(qhltemp,json_mknumber(ped[i].thiscell[j].qhlbar));	
            json_append_element(qhlrmstemp,json_mknumber(ped[i].thiscell[j].qhlrms));	
            json_append_element(qhstemp,json_mknumber(ped[i].thiscell[j].qhsbar));	
            json_append_element(qhsrmstemp,json_mknumber(ped[i].thiscell[j].qhsrms));	
            json_append_element(qlxtemp,json_mknumber(ped[i].thiscell[j].qlxbar));	
            json_append_element(qlxrmstemp,json_mknumber(ped[i].thiscell[j].qlxrms));	
            json_append_element(tactemp,json_mknumber(ped[i].thiscell[j].tacbar));	
            json_append_element(tacrmstemp,json_mknumber(ped[i].thiscell[j].tacrms));	
          }
          json_append_element(num,numtemp);
          json_append_element(qhl,qhltemp);
          json_append_element(qhlrms,qhlrmstemp);
          json_append_element(qhs, qhstemp);
          json_append_element(qhsrms, qhsrmstemp);
          json_append_element(qlx, qlxtemp);
          json_append_element(qlxrms, qlxrmstemp);
          json_append_element(tac, tactemp);
          json_append_element(tacrms, tacrmstemp);
          json_append_element(error_node,json_mkbool(error_flag[i]));
          json_append_element(error_flags,json_mknumber((double)error_flag[i]));
        }
        json_append_member(newdoc,"type",json_mkstring("ped_run"));
        json_append_member(newdoc,"num",num);
        json_append_member(newdoc,"qhl",qhl);
        json_append_member(newdoc,"qhl_rms",qhlrms);
        json_append_member(newdoc,"qhs",qhs);
        json_append_member(newdoc,"qhs_rms",qhsrms);
        json_append_member(newdoc,"qlx",qlx);
        json_append_member(newdoc,"qlx_rms",qlxrms);
        json_append_member(newdoc,"tac",tac);
        json_append_member(newdoc,"tac_rms",tacrms);
        json_append_member(newdoc,"errors",error_node);
        json_append_member(newdoc,"error_flags",error_flags);

        int pass_flag = 1;;
        for (j=0;j<32;j++)
          if (error_flag[j] != 0)
            pass_flag = 0;;
        json_append_member(newdoc,"pass",json_mkbool(pass_flag));
        json_append_member(newdoc,"balanced",json_mkbool(arg.balanced));

        if (arg.final_test){
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        }
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc); // only need to delete the head node
      }

    } // end loop over slot mask
  } // end loop over slots

  free(pmt_buffer);
  free(ped);

  // disable triggers
  unset_ped_crate_mask(MASKALL);
  unset_gt_crate_mask(MASKALL);

  // turn off pedestals
  set_crate_pedestals(arg.crate_num,arg.slot_mask,0x0,&thread_fdset);
  deselect_fecs(arg.crate_num,&thread_fdset);
  if (errors)
    pt_printsend("There were %d errors\n",errors);
  else
    pt_printsend("No errors seen\n");

  pt_printsend("*******************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
  return;
}

