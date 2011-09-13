#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"
#include "xl3_registers.h"
#include "mtc_registers.h"
#include "dac_number.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_utils.h"
#include "fec_utils.h"
#include "crate_cbal.h"

int crate_cbal(char *buffer)
{
  crate_cbal_t *args;
  args = malloc(sizeof(crate_cbal_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->pattern = 0xFFFFFFFF;
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
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: crate_cbal -c [crate num (int)] "
            "-s [slot mask (hex)] -p [channel mask (hex)] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_crate_cbal,(void *)args);
  return 0; 
}


void *pt_crate_cbal(void *args)
{
  crate_cbal_t arg = *(crate_cbal_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  // constants
  const int max_channels = 32;
  const int num_cells = 16;
  const int max_iterations = 40;
  const float acceptable_diff = 10;
  const uint16_t low_dac_initial_setting = 0x32;
  const uint16_t high_dac_initial_setting = 0xBE;
  const int vsi_test_val = 0;
  const int isetm_test_val = 85;
  const int rmp1_test_val = 100;
  const int vli_test_val = 120;
  const int rmp2_test_val = 155;
  const int vmax_test_val = 203;
  const int tacref_test_val = 72;

  uint32_t balanced_chans;
  uint32_t active_chans = arg.pattern;
  uint32_t orig_active_chans;

  struct pedestal x1[32],x2[32],tmp[32];
  struct pedestal x1l[32],x2l[32],tmpl[32];

  int x1_bal[32],x2_bal[32],tmp_bal[32],bestguess_bal[32]; // dac settings
  float f1[32],f2[32]; // the charge that we are balancing

  struct channel_params chan_param[32];

  int iterations,return_value = 0;

  float fmean1,fmean2;
  int vbal_temp[2][32*16];
  uint32_t result;
  uint32_t *pmt_buf;
  int num_dacs;
  uint32_t dac_nums[50];
  uint32_t dac_values[50];

  int error_flags[32];
  int ef;
  for (ef=0;ef<32;ef++)
    error_flags[ef] = 0;

  // malloc
  pmt_buf = (uint32_t *) malloc(0x100000*sizeof(uint32_t));
  if (pmt_buf == NULL){
    pt_printsend("Problem mallocing space for pedestals. Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // set up pulser for soft GT mode
  if (setup_pedestals(0,50,125,0,(0x1<<arg.crate_num)+MSK_TUB,(0x1<<arg.crate_num)+MSK_TUB)){
    pt_printsend("problem setting up pedestals on mtc. Exiting\n");
    free(pmt_buf);
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // loop over slots
  int i,j,k;
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      uint32_t select_reg = FEC_SEL*i;
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0xF,&result,arg.crate_num,&thread_fdset);

      pt_printsend("--------------------------------\n");
      pt_printsend("Balancing Crate %d, Slot %d\n",arg.crate_num,i);

      // initialize variables
      iterations = 0;
      num_dacs = 0;
      balanced_chans = 0x0;
      for (j=0;j<32;j++){
        chan_param[j].test_status = 0x0; // zero means test not passed
        chan_param[j].hi_balanced = 0; // high not balanced
        chan_param[j].low_balanced = 0; // low not balanced
        chan_param[j].high_gain_balance = 0;
        chan_param[j].low_gain_balance = 0;
      }
      orig_active_chans = active_chans;

      // set up timing
      // we will set VSI and VLI to a long time for test
      for (j=0;j<8;j++){
        dac_nums[num_dacs] = d_rmp[j];
        dac_values[num_dacs] = rmp2_test_val;
        num_dacs++;
        dac_nums[num_dacs] = d_rmpup[j];
        dac_values[num_dacs] = rmp1_test_val;
        num_dacs++;
        dac_nums[num_dacs] = d_vsi[j];
        dac_values[num_dacs] = vsi_test_val;
        num_dacs++;
        dac_nums[num_dacs] = d_vli[j];
        dac_values[num_dacs] = vli_test_val;
        num_dacs++;
      }
      // now CMOS timing for GTValid
      for (j=0;j<2;j++){
        dac_nums[num_dacs] = d_isetm[j];
        dac_values[num_dacs] = isetm_test_val;
        num_dacs++;
      }
      dac_nums[num_dacs] = d_tacref;
      dac_values[num_dacs] = tacref_test_val;
      num_dacs++;
      dac_nums[num_dacs] = d_vmax;
      dac_values[num_dacs] = vmax_test_val;
      num_dacs++;
      // now lets load these dacs
      if (multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset) != 0){
        pt_printsend("Error loading dacs. Exiting\n");
        free(pmt_buf);
        unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
        return;
      }
      num_dacs = 0;

      // loop over high and low gain
      // first loop balanced qhs with qhl
      // second loop balances qlx (normal) with qlx (LGI set) 
      int wg;
      for (wg=0;wg<2;wg++){
        // initialize
        active_chans = orig_active_chans;
        // rezero
        for (j=0;j<32;j++){
          f1[j] = 0;
          f2[j] = 0;
          x1_bal[j] = low_dac_initial_setting;
          x2_bal[j] = high_dac_initial_setting;
        }


        // calculate min balance
        // set dacs to minimum
        for (j=0;j<32;j++){
          if ((0x1<<j) & active_chans){
            if (wg == 0)
              dac_nums[num_dacs] = d_vbal_hgain[j];
            else
              dac_nums[num_dacs] = d_vbal_lgain[j];
            dac_values[num_dacs] = x1_bal[j];
            num_dacs++;
          }
        }
        if (multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset)){
          pt_printsend("Error loading dacs. Exiting\n");
          free(pmt_buf);
          unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        num_dacs = 0;
        // get pedestal data
        if (get_pedestal(x1,chan_param,pmt_buf,arg.crate_num,i,arg.pattern,&thread_fdset)){
          pt_printsend("Error during pedestal running or reading. Exiting\n");
          free(pmt_buf);
          unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        // if low gain do again with LGI bit set
        if (wg == 1){
          xl3_rw(CMOS_LGISEL_R + select_reg + WRITE_REG,0x1,&result,arg.crate_num,&thread_fdset); 
          if (get_pedestal(x1l,chan_param,pmt_buf,arg.crate_num,i,arg.pattern,&thread_fdset)){
            pt_printsend("Error during pedestal running or reading. Exiting\n");
            free(pmt_buf);
            unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
            return;
          }
          xl3_rw(CMOS_LGISEL_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset); 
        }

        // calculate max balance
        // set dacs to maximum
        for (j=0;j<32;j++){
          if ((0x1<<j) & active_chans){
            if (wg == 0)
              dac_nums[num_dacs] = d_vbal_hgain[j];
            else
              dac_nums[num_dacs] = d_vbal_lgain[j];
            dac_values[num_dacs] = x2_bal[j];
            num_dacs++;
          }
        }
        if (multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset)){
          pt_printsend("Error loading dacs. Exiting\n");
          free(pmt_buf);
          unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        num_dacs = 0;
        // get pedestal data
        if (get_pedestal(x2,chan_param,pmt_buf,arg.crate_num,i,arg.pattern,&thread_fdset)){
          pt_printsend("Error during pedestal running or reading. Exiting\n");
          free(pmt_buf);
          unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        // if low gain do again with LGI bit set
        if (wg == 1){
          xl3_rw(CMOS_LGISEL_R + select_reg + WRITE_REG,0x1,&result,arg.crate_num,&thread_fdset); 
          if (get_pedestal(x2l,chan_param,pmt_buf,arg.crate_num,i,arg.pattern,&thread_fdset)){
            pt_printsend("Error during pedestal running or reading. Exiting\n");
            free(pmt_buf);
            unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
            return;
          }
          xl3_rw(CMOS_LGISEL_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset); 
        }

        // end setting initial high and low value

        iterations = 0;
        balanced_chans = 0x0;

        // we will now loop until we converge and are balanced
        do{
          // make sure we arent stuck forever
          if (iterations++ > max_iterations){
            //pt_printsend("Too many interations, exiting with some channels unbalanced.\n");
            //pt_printsend("Making best guess for unbalanced channels\n");
            for (j=0;j<32;j++)
              if (wg == 0){
                if (chan_param[j].hi_balanced == 0)
                  chan_param[j].high_gain_balance == bestguess_bal[j];
              }else              
                if (chan_param[j].low_balanced == 0)
                  chan_param[j].low_gain_balance == bestguess_bal[j];
            break;
          }

          // loop over channels
          for (j=0;j<32;j++){
            // if this channel is active and is not yet balanced
            int is_balanced = wg == 0 ? chan_param[j].hi_balanced : chan_param[j].low_balanced;
            if (((0x1<<j) & active_chans) && (is_balanced == 0)){
              fmean1 = 0;
              fmean2 = 0;
              if (wg == 0){
                // find the average difference between qhl and qhs
                for (k=0;k<16;k++){
                  fmean1 += x1[j].thiscell[k].qhlbar-x1[j].thiscell[k].qhsbar;
                  fmean2 += x2[j].thiscell[k].qhlbar-x2[j].thiscell[k].qhsbar;
                }
              }else{
                // find the average difference between qhl and qhs
                for (k=0;k<16;k++){
                  fmean1 += x1[j].thiscell[k].qlxbar-x1l[j].thiscell[k].qlxbar;
                  fmean2 += x2[j].thiscell[k].qlxbar-x2l[j].thiscell[k].qlxbar;
                }
              }
              f1[j] = fmean1/16;
              f2[j] = fmean2/16;
              // make sure we straddle best fit point
              // i.e. the both have the sign on first run
              if (((f1[j]*f2[j]) > 0.0) && (iterations == 1)){
                //pt_printsend("Error: channel %d does not appear balanceable. (%f, %f)\n",
                //    j,f1[j],f2[j]);
                // turn this channel off and go on
                active_chans &= ~(0x1<<j);
                return_value += 100;
              }
              // check if either high or low was balanced
              if (fabs(f2[j]) < acceptable_diff){
                balanced_chans |= 0x1<<j;
                if (wg == 0){
                  chan_param[j].hi_balanced = 1;
                  chan_param[j].high_gain_balance = x2_bal[j];
                }else{
                  chan_param[j].low_balanced = 1;
                  chan_param[j].low_gain_balance = x2_bal[j];
                }
                active_chans &= ~(0x1<<j);
              }else if (fabs(f1[j]) < acceptable_diff){
                balanced_chans |= 0x1<<j;
                if (wg == 0){
                  chan_param[j].hi_balanced = 1;
                  chan_param[j].high_gain_balance = x1_bal[j];
                }else{
                  chan_param[j].low_balanced = 1;
                  chan_param[j].low_gain_balance = x1_bal[j];
                }
                active_chans &= ~(0x1<<j);
              }else{
                // still not balanced
                // pick new points to test
                tmp_bal[j] = x1_bal[j] + (x2_bal[j]-x1_bal[j])*(f1[j]/(f1[j]-f2[j]));

                // keep track of best guess
                if (fabs(f1[j] < fabs(f2[j])))
                  bestguess_bal[j] = x1_bal[j];
                else
                  bestguess_bal[j] = x2_bal[j];

                // make sure we arent stuck
                if (tmp_bal[j] == x2_bal[j]){
                  pt_printsend("channel %d in local trap. Nudging\n");
                  int kick = (int) (rand()%35) + 150;
                  tmp_bal[j] = (tmp_bal[j] >= 45) ? (tmp_bal[j]-kick) : (tmp_bal[j] + kick);
                }

                // make sure we stay withing bounds
                if (tmp_bal[j] > 255)
                  tmp_bal[j] = 255;
                else if (tmp_bal[j] < 0)
                  tmp_bal[j] = 0;

                if (wg == 0)
                  dac_nums[num_dacs] = d_vbal_hgain[j];
                else
                  dac_nums[num_dacs] = d_vbal_lgain[j];
                dac_values[num_dacs] = tmp_bal[j];
                num_dacs++;

                // now we loop through the rest of the channels
                // and build up all the dac values we are going
                // to set before running pedestals again

              } // end if balanced or not
            } // end if active and not balanced
          } // end loop over channels

          // we have new pedestal values for each channel
          // lets load them up
          if (multi_loadsDac(num_dacs,dac_nums,dac_values,arg.crate_num,i,&thread_fdset)){
            pt_printsend("Error loading dacs. Exiting\n");
            free(pmt_buf);
            unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
            return;
          }
          num_dacs = 0;

          // lets do a ped run with the new balance
          // if there are still channels left to go
          if (active_chans != 0x0){
            if (get_pedestal(tmp,chan_param,pmt_buf,arg.crate_num,i,arg.pattern,&thread_fdset)){
              pt_printsend("Error during pedestal running or reading. Exiting\n");
              free(pmt_buf);
              unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
              return;
            }

            if (wg == 1){
              xl3_rw(CMOS_LGISEL_R + select_reg + WRITE_REG,0x1,&result,arg.crate_num,&thread_fdset); 
              if (get_pedestal(tmpl,chan_param,pmt_buf,arg.crate_num,i,arg.pattern,&thread_fdset)){
                pt_printsend("Error during pedestal running or reading. Exiting\n");
                free(pmt_buf);
                unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
                return;
              }
              xl3_rw(CMOS_LGISEL_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset); 
            }

            // now update the two points
            for (j=0;j<32;j++){
              if ((0x1<<j) & active_chans){
                if (wg == 0){
                  x1[j] = x2[j];
                  x1_bal[j] = x2_bal[j];
                  x2[j] = tmp[j];
                  x2_bal[j] = tmp_bal[j];
                }else{
                  x1[j] = x2[j];
                  x1l[j] = x2l[j];
                  x1_bal[j] = x2_bal[j];
                  x2[j] = tmp[j];
                  x2l[j] = tmpl[j];
                  x2_bal[j] = tmp_bal[j];
                }
              }
            }
          } // end if active_chans != 0x0

        } while (balanced_chans != orig_active_chans); // loop until all balanced

      } // end loop over gains

      active_chans = orig_active_chans;
      pt_printsend("\nFinal VBAL table:\n");

      // print out results
      for (j=0;j<32;j++){
        if ((0x1<<j) & active_chans){
          // if fully balanced
          if ((chan_param[j].hi_balanced == 1) && (chan_param[j].low_balanced == 1)){
            pt_printsend("Ch %2i High: %3i. low; %3i. -> Balanced.\n",
                j,chan_param[j].high_gain_balance,chan_param[j].low_gain_balance);
            // check for extreme values
            if ((chan_param[j].high_gain_balance == 255) ||
                (chan_param[j].high_gain_balance == 0)){
              chan_param[j].high_gain_balance = 150;
              pt_printsend(">>>High gain balance extreme, setting to 150.\n");
              error_flags[j] = 1;
            }
            if ((chan_param[j].low_gain_balance == 255) ||
                (chan_param[j].low_gain_balance == 0)){
              chan_param[j].low_gain_balance = 150;
              pt_printsend(">>>High gain balance extreme, setting to 150.\n");
              error_flags[j] = 1;
            }
            if ((chan_param[j].high_gain_balance > 225) ||
                (chan_param[j].high_gain_balance < 50) ||
                (chan_param[j].low_gain_balance > 255) ||
                (chan_param[j].low_gain_balance < 50)){
              pt_printsend(">>> Warning: extreme balance value.\n");
              error_flags[j] = 2;
            }
          }
          // if partially balanced
          else if ((chan_param[j].hi_balanced == 1) || (chan_param[j].low_balanced == 1)){
            error_flags[j] = 3;
            pt_printsend("Ch %2i High: %3i. Low: %3i -> Partially balanced, "
                "will set extreme to 150\n",
                j,chan_param[j].high_gain_balance,chan_param[j].low_gain_balance);
            if ((chan_param[j].high_gain_balance == 255) ||
                (chan_param[j].high_gain_balance == 0))
              chan_param[j].high_gain_balance = 150;
            if ((chan_param[j].low_gain_balance == 255) ||
                (chan_param[j].low_gain_balance == 0))
              chan_param[j].low_gain_balance = 150;
          }
          // if unbalanced
          else{
            error_flags[j] = 4;
            pt_printsend("Ch %2i                       -> Unbalanced, set to 150\n",j);
            chan_param[j].high_gain_balance = 150;
            chan_param[j].low_gain_balance = 150;
            return_value++;
          } // end switch over balanced state
        } // end if active chan
      } // end loop over chans

      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0xF,&result,arg.crate_num,&thread_fdset);
      deselect_fecs(arg.crate_num,&thread_fdset);

      // now update database
      if (arg.update_db){
        pt_printsend("updating the database\n");
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("crate_cbal"));
        JsonNode* vbal_high_new = json_mkarray();
        JsonNode* vbal_low_new = json_mkarray();
        JsonNode* error_new = json_mkarray();
        JsonNode* chan_error = json_mkarray();
        int pass_flag = 1;
        for (j=0;j<32;j++){
          json_append_element(vbal_high_new,
              json_mknumber((double)chan_param[j].high_gain_balance));
          json_append_element(vbal_low_new,
              json_mknumber((double)chan_param[j].low_gain_balance));
          json_append_element(chan_error,json_mkbool(error_flags[j]));
          if (error_flags[j] == 0)
            json_append_element(error_new,json_mkstring("none"));
          else if (error_flags[j] == 1)
            json_append_element(error_new,json_mkstring("Extreme balance set to 150"));
          else if (error_flags[j] == 2)
            json_append_element(error_new,json_mkstring("Extreme balance values"));
          else if (error_flags[j] == 3)
            json_append_element(error_new,json_mkstring("Partially balanced"));
          else if (error_flags[j] == 4)
            json_append_element(error_new,json_mkstring("Unbalanced, set to 150"));
          if (error_flags[j] != 0)
            pass_flag = 0;
        }
        if (return_value != 0)
          pass_flag = 0;
        json_append_member(newdoc,"vbal_low",vbal_low_new);
        json_append_member(newdoc,"vbal_high",vbal_high_new);
        json_append_member(newdoc,"error_messages",error_new);
        json_append_member(newdoc,"errors",chan_error);
        json_append_member(newdoc,"pass",json_mkbool(pass_flag));

        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[j]));	
        post_debug_doc(arg.crate_num,j,newdoc,&thread_fdset);
        json_delete(newdoc); // delete the head
      }


    } // end if slot mask
  } // end loop over slots

  pt_printsend("End of crate_cbal\n");
  pt_printsend("********************************\n");

  free(pmt_buf);
  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

int get_pedestal(struct pedestal *pedestals, struct channel_params *chan_params, uint32_t *pmt_buf, int crate, int slot, uint32_t pattern, fd_set *thread_fdset)
{
  pt_printsend(".");
  fflush(stdout);

  int i,j;
  uint32_t result;
  uint32_t select_reg = slot*FEC_SEL;

  float max_rms_qlx = 2.0;
  float max_rms_qhl = 2.0;
  float max_rms_qhs = 10.0;
  float max_rms_tac = 3.0;
  int num_pulses = 25*16;
  int min_num_words = 0;
  for (i=0;i<32;i++)
    if ((0x1<<i) & pattern)
      min_num_words += (num_pulses-25)*3; //??
  int max_errors = 250;

  int words_in_mem;
  FEC32PMTBundle pmt_data;

  if (pedestals == 0){
    pt_printsend("Error: null pointer passed to get_pedestal! Exiting\n");
    return 666;
  }
  for (i=0;i<32;i++){
    pedestals[i].channelnumber = i;
    pedestals[i].per_channel = 0;
    for (j=0;j<16;j++){
      pedestals[i].thiscell[j].cellno = j;
      pedestals[i].thiscell[j].per_cell = 0;
      pedestals[i].thiscell[j].qlxbar = 0;
      pedestals[i].thiscell[j].qlxrms = 0;
      pedestals[i].thiscell[j].qhlbar = 0;
      pedestals[i].thiscell[j].qhlbar = 0;
      pedestals[i].thiscell[j].qhsrms = 0;
      pedestals[i].thiscell[j].qhsrms = 0;
      pedestals[i].thiscell[j].tacbar = 0;
      pedestals[i].thiscell[j].tacrms = 0;
    }
  }

  // end initialization

  // reset board - not full reset, just CMOS and fifo
  xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0xE,&result,crate,thread_fdset);
  xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result,crate,thread_fdset);
  // enable the appropriate pedestals
  xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,pattern,&result,crate,thread_fdset);

  // now pulse the pulser num_pulses times
  multi_softgt(num_pulses);

  // sleep so that sequencer has time to do its shit
  usleep(5000);

  // check that we have enough data in memory
  xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&result,crate,thread_fdset);
  result &= 0x000FFFFF;

  // if there are less bundles than there are supposed to be
  // then read out whats there minus a fudge factor ??
  if (result <= 32*3*num_pulses)
    words_in_mem = result > 100 ? result-100 : result;
  else
    words_in_mem = 32*3*num_pulses;

  // if it is too low, abort
  if (words_in_mem < min_num_words){
    pt_printsend("Less than %d bundles in memory (there are %d). Exiting\n",
        min_num_words,words_in_mem);
    return 10;
  }

  // now read out memory
  XL3_Packet packet;
  read_pedestals_args_t *packet_args = (read_pedestals_args_t *) packet.payload;
  int reads_left = words_in_mem;
  int this_read;
  while (reads_left != 0){
    if (reads_left > MAX_FEC_COMMANDS-1000)
      this_read = MAX_FEC_COMMANDS-1000;
    else
      this_read = reads_left;
    packet.cmdHeader.packet_type = READ_PEDESTALS_ID;
    packet_args->slot = slot;
    packet_args->reads = this_read;
    SwapLongBlock(packet_args,sizeof(read_pedestals_args_t)/sizeof(uint32_t));
    do_xl3_cmd(&packet,crate,thread_fdset);

    // now wait for the data to come
    wait_for_multifc_results(this_read,command_number[crate]-1,crate,pmt_buf+(words_in_mem-reads_left),thread_fdset);
    reads_left -= this_read;
  }

  // parse charge information
  for (i=0;i<words_in_mem;i+=3){
    pmt_data = MakeBundleFromData(pmt_buf+i);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qlxbar += pmt_data.ADC_Qlx;
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhlbar += pmt_data.ADC_Qhl;
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhsbar += pmt_data.ADC_Qhs;
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].tacbar += pmt_data.ADC_TAC;

    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qlxrms +=
      pow(pmt_data.ADC_Qlx,2);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhlrms +=
      pow(pmt_data.ADC_Qhl,2);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhsrms +=
      pow(pmt_data.ADC_Qhs,2);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].tacrms +=
      pow(pmt_data.ADC_TAC,2);

    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].per_cell++;
    pedestals[pmt_data.ChannelID].per_channel++;
  }

  // final step of calculation
  for (i=0;i<32;i++){
    if (pedestals[i].per_channel > 0){
      chan_params[i].test_status |= PED_TEST_TAKEN | PED_CH_HAS_PEDESTALS |
        PED_RMS_TEST_PASSED | PED_PED_WITHIN_RANGE;
      chan_params[i].test_status &= ~(PED_DATA_NO_ENABLE | PED_TOO_FEW_PER_CELL);

      for (j=0;j<16;j++){
        int num_events = pedestals[i].thiscell[j].per_cell;
        if (((num_events*16.0/num_pulses) > 1.1) || (num_events*16.0/num_pulses < 0.9)){
          chan_params[i].test_status |= PED_TOO_FEW_PER_CELL;
          continue;
        }
        if (num_events > 1){
          pedestals[i].thiscell[j].qlxbar = pedestals[i].thiscell[j].qlxbar / num_events;
          pedestals[i].thiscell[j].qhlbar = pedestals[i].thiscell[j].qhlbar / num_events;
          pedestals[i].thiscell[j].qhsbar = pedestals[i].thiscell[j].qhsbar / num_events;
          pedestals[i].thiscell[j].tacbar = pedestals[i].thiscell[j].tacbar / num_events;

          pedestals[i].thiscell[j].qlxrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].qlxrms / num_events -
             pow(pedestals[i].thiscell[j].qlxbar,2));
          pedestals[i].thiscell[j].qhlrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].qhlrms / num_events -
             pow(pedestals[i].thiscell[j].qhlbar,2));
          pedestals[i].thiscell[j].qhsrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].qhsrms / num_events -
             pow(pedestals[i].thiscell[j].qhsbar,2));
          pedestals[i].thiscell[j].tacrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].tacrms / num_events -
             pow(pedestals[i].thiscell[j].tacbar,2));

          pedestals[i].thiscell[j].qlxrms = sqrt(pedestals[i].thiscell[j].qlxrms);
          pedestals[i].thiscell[j].qhlrms = sqrt(pedestals[i].thiscell[j].qhlrms);
          pedestals[i].thiscell[j].qhsrms = sqrt(pedestals[i].thiscell[j].qhsrms);
          pedestals[i].thiscell[j].tacrms = sqrt(pedestals[i].thiscell[j].tacrms);

          if ((pedestals[i].thiscell[j].qlxrms > max_rms_qlx) ||
              (pedestals[i].thiscell[j].qhlrms > max_rms_qhl) ||
              (pedestals[i].thiscell[j].qhsrms > max_rms_qhs) ||
              (pedestals[i].thiscell[j].tacrms > max_rms_tac))
            chan_params[i].test_status &= ~PED_RMS_TEST_PASSED;
        }else{
          chan_params[i].test_status &= ~PED_CH_HAS_PEDESTALS;
        }
      } // end loop over cells
    } // end if channel has pedestals
  } // end loop over channels
  return 0;
}

FEC32PMTBundle MakeBundleFromData(uint32_t *buffer)
{
  short i;
  unsigned short triggerWord2;

  unsigned short ValADC0, ValADC1, ValADC2, ValADC3;
  unsigned short signbit0, signbit1, signbit2, signbit3;
  FEC32PMTBundle GetBundle;

  /*initialize PMTBundle to all zeros */
  // display the lower and the upper bits separately
  GetBundle.GlobalTriggerID = 0;
  GetBundle.GlobalTriggerID2 = 0;
  GetBundle.ChannelID = 0;
  GetBundle.CrateID = 0;
  GetBundle.BoardID = 0;
  GetBundle.CMOSCellID = 0;
  GetBundle.ADC_Qlx = 0;
  GetBundle.ADC_Qhs = 0;
  GetBundle.ADC_Qhl = 0;
  GetBundle.ADC_TAC = 0;
  GetBundle.CGT_ES16 = 0;
  GetBundle.CGT_ES24 = 0;
  GetBundle.Missed_Count = 0;
  GetBundle.NC_CC = 0;
  GetBundle.LGI_Select = 0;
  GetBundle.CMOS_ES16 = 0;

  GetBundle.BoardID = sGetBits(*buffer, 29, 4);		// FEC32 card ID

  GetBundle.CrateID = sGetBits(*buffer, 25, 5);		// Crate ID

  GetBundle.ChannelID = sGetBits(*buffer, 20, 5);	// CMOS Channel ID


  // lower 16 bits of global trigger ID
  //triggerWord = sGetBits(TempVal,15,16);  

  // lower 16 bits of the                       
  GetBundle.GlobalTriggerID = sGetBits(*buffer, 15, 16);
  // global trigger ID
  GetBundle.CGT_ES16 = sGetBits(*buffer, 30, 1);
  GetBundle.CGT_ES24 = sGetBits(*buffer, 31, 1);

  // now get ADC output and peel off the corresponding values and
  // convert to decimal
  // ADC0= Q_low gain,  long integrate (Qlx)
  // ADC1= Q_high gain, short integrate time (Qhs)
  // ADC2= Q_high gain, long integrate time  (Qhl)
  // ADC3= TAC

  GetBundle.CMOSCellID = sGetBits(*(buffer+1), 15, 4);	// CMOS Cell number

  signbit0 = sGetBits(*(buffer+1), 11, 1);
  signbit1 = sGetBits(*(buffer+1), 27, 1);
  ValADC0 = sGetBits(*(buffer+1), 10, 11);
  ValADC1 = sGetBits(*(buffer+1), 26, 11);

  // ADC values are in 2s complement code 
  if (signbit0 == 1)
    ValADC0 = ValADC0 - 2048;
  if (signbit1 == 1)
    ValADC1 = ValADC1 - 2048;

  //Add 2048 offset to ADC0-1 so range is from 0 to 4095 and unsigned
  GetBundle.ADC_Qlx = (unsigned short) (ValADC0 + 2048);
  GetBundle.ADC_Qhs = (unsigned short) (ValADC1 + 2048);

  GetBundle.Missed_Count = sGetBits(*(buffer+1), 28, 1);;
  GetBundle.NC_CC = sGetBits(*(buffer+1), 29, 1);;
  GetBundle.LGI_Select = sGetBits(*(buffer+1), 30, 1);;
  GetBundle.CMOS_ES16 = sGetBits(*(buffer+1), 31, 1);;

  signbit2 = sGetBits(*(buffer+2), 11, 1);
  signbit3 = sGetBits(*(buffer+2), 27, 1);
  ValADC2 = sGetBits(*(buffer+2), 10, 11);
  ValADC3 = sGetBits(*(buffer+2), 26, 11);

  // --------------- the full concatanated global trigger ID --------------
  //            for (i = 4; i >= 1 ; i--){
  //                    if ( sGetBits(TempVal,(15 - i + 1),1) )
  //                            triggerWord |= ( 1UL << (19 - i + 1) );
  //            }
  //            for (i = 4; i >= 1 ; i--){
  //                    if ( sGetBits(TempVal,(31 - i + 1),1) )
  //                            triggerWord |= ( 1UL << (23 - i + 1) );
  //            }
  // --------------- the full concatanated global trigger ID --------------

  triggerWord2 = sGetBits(*(buffer+2), 15, 4);	// Global Trigger bits 19-16

  for (i = 4; i >= 1; i--) {
    if (sGetBits(*(buffer+2), (31 - i + 1), 1))
      triggerWord2 |= (1UL << (7 - i + 1));
  }

  // upper 8 bits of Global Trigger ID 5/27/96
  GetBundle.GlobalTriggerID2 = triggerWord2;

  // ADC values are in 2s complement code 
  if (signbit2 == 1)
    ValADC2 = ValADC2 - 2048;
  if (signbit3 == 1)
    ValADC3 = ValADC3 - 2048;

  // Add 2048 offset to ADC2-3 so range is from 0 to 4095 and unsigned
  GetBundle.ADC_Qhl = (unsigned short) (ValADC2 + 2048);
  GetBundle.ADC_TAC = (unsigned short) (ValADC3 + 2048);

  return GetBundle;
}


