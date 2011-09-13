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
#include "daq_utils.h"
#include "mtc_utils.h"
#include "fec_utils.h"
#include "chinj_scan.h"

#define HV_BIT_COUNT 40
#define HV_HVREF_DAC 136

int chinj_scan(char *buffer)
{
  chinj_scan_t *args;
  args = malloc(sizeof(chinj_scan_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->frequency = 0;
  args->pattern = 0xFFFFFFFF;
  args->gtdelay = DEFAULT_GT_DELAY;
  args->ped_width = DEFAULT_PED_WIDTH;
  args->num_pedestals = 50;
  args->chinj_lower = 500;
  args->chinj_upper = 800;
  args->q_select = 0;
  args->ped_on = 0;
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
      }else if (words[1] == 'w'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->ped_width = atoi(words2);
      }else if (words[1] == 't'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->gtdelay = atoi(words2);
      }else if (words[1] == 'n'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->num_pedestals = atoi(words2);
      }else if (words[1] == 'l'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->chinj_lower = atof(words2);
      }else if (words[1] == 'u'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->chinj_upper = atof(words2);
      }else if (words[1] == 'a'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->q_select = atoi(words2);
      }else if (words[1] == 'f'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->frequency = atof(words2);
      }else if (words[1] == 'e'){args->ped_on = 1;
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: chinj_scan -c [crate num (int)] "
            "-s [slot mask (hex)] -p [channel mask (hex)] "
            "-f [frequency] -t [gtdelay] -w [ped with] -n [num pedestals] "
            "-l [charge lower limit] -u [charge upper limit] "
            "-a [charge select (0=qhl,1=qhs,2=qlx,3=tac)] "
            "-e (enable pedestal) -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_chinj_scan,(void *)args);
  return 0; 
}


void *pt_chinj_scan(void *args)
{
  chinj_scan_t arg = *(chinj_scan_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  pt_printsend("Charge Injection Run Setup\n");
  pt_printsend("-------------------------------------------\n");
  pt_printsend("Crate:		    %2d\n",arg.crate_num);
  pt_printsend("Slot Mask:		    0x%4hx\n",arg.slot_mask);
  pt_printsend("Pedestal Mask:	    0x%08lx\n",arg.pattern);
  pt_printsend("GT delay (ns):	    %3hu\n", arg.gtdelay);
  pt_printsend("Pedestal Width (ns):    %2d\n",arg.ped_width);
  pt_printsend("Pulser Frequency (Hz):  %3.0f\n",arg.frequency);

  float qhls[16*32*2][26];
  float qhss[16*32*2][26];
  float qlxs[16*32*2][26];
  float tacs[16*32*2][26];
  int scan_errors[16*32*2][26];
  int count,crateID,ch,cell,num_events;
  uint16_t dacvalue;
  uint32_t *pmt_buffer,*pmt_iter;
  struct pedestal *ped;
  uint32_t result, select_reg;
  int errors;
  uint32_t default_ch_mask;
  int chinj_err[16];

  pmt_buffer = malloc( 0x100000*sizeof(uint32_t));
  ped = malloc( 32 * sizeof(struct pedestal));
  if (pmt_buffer == NULL || ped == NULL){
    pt_printsend("Problem mallocing! Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  int i,j,k;
  for (i=0;i<16;i++){
    chinj_err[i] = 0;
  }
  int dac_iter;

  for (dac_iter=0;dac_iter<26;dac_iter++){

    dacvalue = dac_iter*10;

    int slot_iter;
    for (slot_iter = 0; slot_iter < 16; slot_iter ++){
      if ((0x1 << slot_iter) & arg.slot_mask){
        select_reg = FEC_SEL*slot_iter;
        xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result,arg.crate_num,&thread_fdset);
        xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result,arg.crate_num,&thread_fdset);
        xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);
        xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,~arg.pattern,&result,arg.crate_num,&thread_fdset);
      }
    }
    deselect_fecs(arg.crate_num,&thread_fdset); 

    //pt_printsend("Reset FECs\n");

    errors = 0;
    errors += fec_load_crate_addr(arg.crate_num,arg.slot_mask,&thread_fdset);
    if (arg.ped_on == 1){
      //pt_printsend("not enabling pedestals.\n");
    }else if (arg.ped_on == 0){
      //pt_printsend("enabling pedestals.\n");
      errors += set_crate_pedestals(arg.crate_num, arg.slot_mask, arg.pattern,&thread_fdset);
    }
    deselect_fecs(arg.crate_num,&thread_fdset);

    if (errors){
      pt_printsend("error setting up FEC crate for pedestals. Exiting.\n");
      free(pmt_buffer);
      free(ped);
      unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
      return;
    }

    //setup charge injection
    default_ch_mask = arg.pattern;
    setup_chinj(arg.crate_num,arg.slot_mask,default_ch_mask,dacvalue,&thread_fdset);

    errors = setup_pedestals(0,arg.ped_width,arg.gtdelay,DEFAULT_GT_FINE_DELAY,(0x1<<arg.crate_num),(0x1<<arg.crate_num));
    if (errors){
      pt_printsend("Error setting up MTC for pedestals. Exiting.\n");
      unset_ped_crate_mask(MASKALL);
      unset_gt_crate_mask(MASKALL);
      free(pmt_buffer);
      free(ped);
      unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
      return;
    }

    // send the softgts
    multi_softgt(arg.num_pedestals*16);

    // LOOP OVER SLOTS
    for (slot_iter = 0; slot_iter < 16; slot_iter ++){
      if ((0x1<<slot_iter) & arg.slot_mask){

        // initialize pedestal struct
        for (i=0;i<32;i++){
          //pedestal struct
          ped[i].channelnumber = i; //channel numbers start at 0!!!
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


        /////////////////////
        // READOUT BUNDLES //
        /////////////////////

        count = read_out_bundles(arg.crate_num, slot_iter, arg.num_pedestals*32*16,pmt_buffer,&thread_fdset);

        //check for readout errors
        if (count <= 0){
          pt_printsend("there was an error in the count!\n");
          pt_printsend("Errors reading out MB(%2d) (errno %i)\n", slot_iter, count);
          errors+=1;
          continue;
        }else{
          //pt_printsend("MB(%2d): %5d bundles read out.\n", slot_iter, count);
        }

        if (count < arg.num_pedestals*32*16)
          errors += 1;

        //process data
        pmt_iter = pmt_buffer;

        for (i=0;i<count;i++){
          crateID = (int) UNPK_CRATE_ID(pmt_iter);
          if (crateID != arg.crate_num){
            pt_printsend("Invalid crate ID seen! (crate ID %2d, bundle %2i)\n", crateID, i);
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
              }
            }
          }
        }

        ///////////////////
        // PRINT RESULTS //
        ///////////////////

        pt_printsend("########################################################\n");
        pt_printsend("Slot (%2d)\n", slot_iter);
        pt_printsend("########################################################\n");

        for (i = 0; i<32; i++){
          //pt_printsend("Ch Cell  #   Qhl         Qhs         Qlx         TAC\n");
          for (j=0;j<16;j++){
            if (j == 0){
              scan_errors[slot_iter*32+i*2][dac_iter] = 0;
              qhls[slot_iter*32+i*2][dac_iter] = ped[i].thiscell[j].qhlbar;
              qhss[slot_iter*32+i*2][dac_iter] = ped[i].thiscell[j].qhsbar;
              qlxs[slot_iter*32+i*2][dac_iter] = ped[i].thiscell[j].qlxbar;
              tacs[slot_iter*32+i*2][dac_iter] = ped[i].thiscell[j].tacbar;
            }
            if (j == 1){
              scan_errors[slot_iter*32+i*2][dac_iter] = 0;
              qhls[slot_iter*32+i*2+1][dac_iter] = ped[i].thiscell[j].qhlbar;
              qhss[slot_iter*32+i*2+1][dac_iter] = ped[i].thiscell[j].qhsbar;
              qlxs[slot_iter*32+i*2+1][dac_iter] = ped[i].thiscell[j].qlxbar;
              tacs[slot_iter*32+i*2+1][dac_iter] = ped[i].thiscell[j].tacbar;
            }
            if (arg.q_select == 0){
              if (ped[i].thiscell[j].qhlbar < arg.chinj_lower ||
                  ped[i].thiscell[j].qhlbar > arg.chinj_upper) {
                chinj_err[slot_iter]++;
                //pt_printsend(">>>>>Qhl Extreme Value<<<<<\n");
                if (j%2 == 0)
                  scan_errors[slot_iter*32+i*2][dac_iter]++;
                else
                  scan_errors[slot_iter*32+i*2+1][dac_iter]++;
              }
            }
            else if (arg.q_select == 1){
              if (ped[i].thiscell[j].qhsbar < arg.chinj_lower ||
                  ped[i].thiscell[j].qhsbar > arg.chinj_upper) {
                chinj_err[slot_iter]++;
                //pt_printsend(">>>>>Qhs Extreme Value<<<<<\n");
                if (j%2 == 0)
                  scan_errors[slot_iter*32+i*2][dac_iter]++;
                else
                  scan_errors[slot_iter*32+i*2+1][dac_iter]++;
              }
            }
            else if (arg.q_select == 2){
              if (ped[i].thiscell[j].qlxbar < arg.chinj_lower ||
                  ped[i].thiscell[j].qlxbar > arg.chinj_upper) {
                chinj_err[slot_iter]++;
                //pt_printsend(">>>>>Qlx Extreme Value<<<<<\n");
                if (j%2 == 0)
                  scan_errors[slot_iter*32+i*2][dac_iter]++;
                else
                  scan_errors[slot_iter*32+i*2+1][dac_iter]++;
              }
            }
            //pt_printsend("%2d %3d %4d %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f\n",
            //i,j,ped[i].thiscell[j].per_cell,
            //ped[i].thiscell[j].qhlbar, ped[i].thiscell[j].qhlrms,
            //ped[i].thiscell[j].qhsbar, ped[i].thiscell[j].qhsrms,
            //ped[i].thiscell[j].qlxbar, ped[i].thiscell[j].qlxrms,
            //ped[i].thiscell[j].tacbar, ped[i].thiscell[j].tacrms);
          }
        }

      } // end if slotmask
    } // end loop over slots

    /*
       if (arg.q_select == 0){
       pt_printsend("Qhl lower, Upper bounds = %f %f\n",arg.chinj_lower,arg.chinj_upper);
       pt_printsend("Number of Qhl overflows = %d\n",chinj_err[slot_iter]);
       }
       else if (arg.q_select == 1){
       pt_printsend("Qhs lower, Upper bounds = %f %f\n",arg.chinj_lower,arg.chinj_upper);
       pt_printsend("Number of Qhs overflows = %d\n",chinj_err[slot_iter]);
       }
       else if (arg.q_select == 2){
       pt_printsend("Qlx lower, Upper bounds = %f %f\n",arg.chinj_lower,arg.chinj_upper);
       pt_printsend("Number of Qlx overflows = %d\n",chinj_err[slot_iter]);
       }
     */

    free(pmt_buffer);
    free(ped);

    //disable trigger enables
    unset_ped_crate_mask(MASKALL);
    unset_gt_crate_mask(MASKALL);

    //unset pedestalenable
    errors += set_crate_pedestals(arg.crate_num, arg.slot_mask, 0x0,&thread_fdset);

    deselect_fecs(arg.crate_num,&thread_fdset);
  } // end loop over dacvalue


  // lets update this database
  if (arg.update_db){
    pt_printsend("updating the database\n");
    for (i=0;i<16;i++)
    {
      if ((0x1<<i) & arg.slot_mask){
        JsonNode *newdoc = json_mkobject();
        JsonNode *qhl_even = json_mkarray();
        JsonNode *qhl_odd = json_mkarray();
        JsonNode *qhs_even = json_mkarray();
        JsonNode *qhs_odd = json_mkarray();
        JsonNode *qlx_even = json_mkarray();
        JsonNode *qlx_odd = json_mkarray();
        JsonNode *tac_even = json_mkarray();
        JsonNode *tac_odd = json_mkarray();
        JsonNode *error_even = json_mkarray();
        JsonNode *error_odd = json_mkarray();
        for (j=0;j<32;j++){
          JsonNode *qhleventemp = json_mkarray();
          JsonNode *qhloddtemp = json_mkarray();
          JsonNode *qhseventemp = json_mkarray();
          JsonNode *qhsoddtemp = json_mkarray();
          JsonNode *qlxeventemp = json_mkarray();
          JsonNode *qlxoddtemp = json_mkarray();
          JsonNode *taceventemp = json_mkarray();
          JsonNode *tacoddtemp = json_mkarray();
          JsonNode *erroreventemp = json_mkarray();
          JsonNode *erroroddtemp = json_mkarray();
          for (k=0;k<26;k++){
            json_append_element(qhleventemp,json_mknumber(qhls[i*32+j*2][k]));	
            json_append_element(qhloddtemp,json_mknumber(qhls[i*32+j*2+1][k]));	
            json_append_element(qhseventemp,json_mknumber(qhss[i*32+j*2][k]));	
            json_append_element(qhsoddtemp,json_mknumber(qhss[i*32+j*2+1][k]));	
            json_append_element(qlxeventemp,json_mknumber(qlxs[i*32+j*2][k]));	
            json_append_element(qlxoddtemp,json_mknumber(qlxs[i*32+j*2+1][k]));	
            json_append_element(taceventemp,json_mknumber(tacs[i*32+j*2][k]));	
            json_append_element(tacoddtemp,json_mknumber(tacs[i*32+j*2+1][k]));	
            json_append_element(erroreventemp,json_mknumber((double)scan_errors[i*32+j*2][k]));	
            json_append_element(erroroddtemp,json_mknumber((double)scan_errors[i*32+j*2+1][k]));	
          }
          json_append_element(qhl_even,qhleventemp);
          json_append_element(qhl_odd,qhloddtemp);
          json_append_element(qhs_even,qhseventemp);
          json_append_element(qhs_odd,qhsoddtemp);
          json_append_element(qlx_even,qlxeventemp);
          json_append_element(qlx_odd,qlxoddtemp);
          json_append_element(tac_even,taceventemp);
          json_append_element(tac_odd,tacoddtemp);
          json_append_element(error_even,erroreventemp);
          json_append_element(error_odd,erroroddtemp);
        }
        json_append_member(newdoc,"type",json_mkstring("chinj_scan"));
        json_append_member(newdoc,"QHL_even",qhl_even);
        json_append_member(newdoc,"QHL_odd",qhl_odd);
        json_append_member(newdoc,"QHS_even",qhs_even);
        json_append_member(newdoc,"QHS_odd",qhs_odd);
        json_append_member(newdoc,"QLX_even",qlx_even);
        json_append_member(newdoc,"QLX_odd",qlx_odd);
        json_append_member(newdoc,"TAC_even",tac_even);
        json_append_member(newdoc,"TAC_odd",tac_odd);
        json_append_member(newdoc,"errors_even",error_even);
        json_append_member(newdoc,"errors_odd",error_odd);
        json_append_member(newdoc,"pass",json_mkbool(!(chinj_err[i])));
        if (arg.final_test){
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	
        }
        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
      }
    }
  }


  if (errors)
    pt_printsend("There were %d errors\n", errors);
  else
    pt_printsend("No errors seen\n");
  pt_printsend("*************************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}


// sets up charge injection by clocking in a 40 bit stream. The last 32 bits set the channels.
// We then set dac 136, the chinj dac
int setup_chinj(int crate, uint16_t slot_mask, uint32_t default_ch_mask, uint16_t dacvalue, fd_set *thread_fdset)
{
  int slot_iter,bit_iter;
  uint32_t amask,word;
  uint32_t select_reg;
  uint32_t result;
  int error;

  amask = default_ch_mask;

  for (slot_iter=0;slot_iter<16;slot_iter++){
    if ((0x1<<slot_iter) & slot_mask){
      select_reg = FEC_SEL*slot_iter;
      for (bit_iter = HV_BIT_COUNT;bit_iter>0;bit_iter--){
        if (bit_iter > 32){
          word = 0x0;
        }else{
          // set bit iff it is set in amask
          word = ((0x1 << (bit_iter -1)) & amask) ? HV_CSR_DATIN : 0x0;
        }
        xl3_rw(FEC_HV_CSR_R + select_reg + WRITE_REG,word,&result,crate,thread_fdset);
        word |= HV_CSR_CLK;
        xl3_rw(FEC_HV_CSR_R + select_reg + WRITE_REG,word,&result,crate,thread_fdset);
      } // end loop over bits

      // now toggle HVLOAD
      xl3_rw(FEC_HV_CSR_R + select_reg + WRITE_REG,0x0,&result,crate,thread_fdset);
      xl3_rw(FEC_HV_CSR_R + select_reg + WRITE_REG,HV_CSR_LOAD,&result,crate,thread_fdset);

      // now set the dac
      error = loadsDac(HV_HVREF_DAC,dacvalue,crate,slot_iter,thread_fdset);
      if (error){
        printsend("setup_chinj: error loading charge injection dac\n");
      }
    } // end if slot_mask
  } // end loop over slots
  deselect_fecs(crate,thread_fdset);
  return 0;
}
