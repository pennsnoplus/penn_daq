#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"
#include "xl3_registers.h"
#include "dac_number.h"
#include "unpack_bundles.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "mtc_utils.h"
#include "fec_utils.h"
#include "daq_utils.h"
#include "cmos_m_gtvalid_old.h"

#define NGTVALID 20
#define GTMAX 1000
#define GTPED_DELAY 20
#define TDELAY_EXTRA 0

#define VMAX 203
#define TACREF 72
#define ISETM_START 147
//#define ISETM 138
#define ISETM 110
#define ISETA 0


int cmos_m_gtvalid_old(char *buffer)
{
  cmos_m_gtvalid_old_t *args;
  args = malloc(sizeof(cmos_m_gtvalid_old_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->pattern = 0xFFFFFFFF;
  args->gt_cutoff = 0;
  args->do_twiddle = 1;
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
      }else if (words[1] == 'g'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->gt_cutoff = atof(words2);
      }else if (words[1] == 'n'){args->do_twiddle = 0;
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
        printsend("Usage: cmos_m_gtvalid_old -c [crate num (int)] "
            "-s [slot mask (hex)] -p [channel mask (hex)] "
            "-g [gt cutoff] -n (no twiddle) -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_cmos_m_gtvalid_old,(void *)args);
  return 0; 
}


void *pt_cmos_m_gtvalid_old(void *args)
{
  cmos_m_gtvalid_old_t arg = *(cmos_m_gtvalid_old_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  int i,j,k;
  uint32_t result;
  int error;
  XL3_Packet packet;

  uint16_t isetm_new[2], tacbits[32], tacbits_new[2][32],gtflag[2][32];
  uint16_t isetm_save[2], tacbits_save[2][32], isetm_start[2];
  uint32_t nfifo, nget;
  uint16_t chan_max_set[2], chan_min_set[2], cmax[2], cmin[2];
  float gtchan[32], gtchan_set[2][32];
  float gtdelay, gtdelta, gtstart, corr_gtdelta, gt_max;
  float gt_temp, gt_max_sec, gt_min, gt_other;
  float gt_max_set[2], gt_min_set[2], gt_start[2][32];
  float ratio, best[32], gmax[2], gmin[2];
  uint16_t chan_max, chan_max_sec, chan_min;
  uint16_t ncrates;
  int done;
  uint16_t slot_errors;
  int chan_errors[32];
  float gt_temp0, gt_temp1;

  // setup crate
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      uint32_t select_reg = FEC_SEL*i;
      // disable pedestals
      xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);
      // reset fifo
      xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,
          &result,arg.crate_num,&thread_fdset);
      xl3_rw(GENERAL_CSR_R + select_reg + READ_REG,0x0,&result,arg.crate_num,&thread_fdset);
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,
          result | (arg.crate_num << FEC_CSR_CRATE_OFFSET) | 0x6,
          &result,arg.crate_num,&thread_fdset);
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,
          (arg.crate_num << FEC_CSR_CRATE_OFFSET),
          &result,arg.crate_num,&thread_fdset);
      xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,
          &result,arg.crate_num,&thread_fdset);
    }
  }
  deselect_fecs(arg.crate_num,&thread_fdset);

  if (setup_pedestals(0,DEFAULT_PED_WIDTH,10,0,(0x1<<arg.crate_num),(0x1<<arg.crate_num))){
    pt_printsend("Error setting up mtc. Exiting\n");
    unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  // loop over slot
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      uint32_t select_reg = FEC_SEL*i;

      slot_errors = 0;
      for (j=0;j<32;j++)
        chan_errors[j] = 0;

      // select which tac we are working on
      int wt;
      for (wt=0;wt<2;wt++){

        // only set dacs if request setting gtvalid
        if (arg.gt_cutoff != 0){
          // set tac reference voltages
          error = loadsDac(d_vmax,VMAX,arg.crate_num,i,&thread_fdset);
          error+= loadsDac(d_tacref,TACREF,arg.crate_num,i,&thread_fdset);
          error+= loadsDac(d_isetm[0],ISETM_START,arg.crate_num,i,&thread_fdset);
          error+= loadsDac(d_isetm[1],ISETM_START,arg.crate_num,i,&thread_fdset);

          // enable TAC secondary current source and
          // adjust bits for all channels. The reason for
          // this is so we can turn bits off to shorten the
          // TAC time
          error+= loadsDac(d_iseta[0],ISETA,arg.crate_num,i,&thread_fdset);
          error+= loadsDac(d_iseta[1],ISETA,arg.crate_num,i,&thread_fdset);
          if (error){
            pt_printsend("Error setting up TAC voltages. Exiting\n");
            unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
            return;
          }
          pt_printsend("Dacs loaded\n");

          // load cmos shift register to enable twiddle bits
          packet.cmdHeader.packet_type = LOADTACBITS_ID;
          loadtacbits_args_t *packet_args = (loadtacbits_args_t *) packet.payload;
          packet_args->crate_num = arg.crate_num;
          packet_args->select_reg = select_reg;
          for (j=0;j<32;j++){
            tacbits[j] = 0x00;
            packet_args->tacbits[j] = tacbits[j];
          }
          SwapLongBlock(packet.payload,2);
          SwapShortBlock(packet.payload+8,32);
          do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
          pt_printsend("TAC bits loaded\n");
        }

        // some other initialization
        isetm_new[0] = ISETM;
        isetm_new[1] = ISETM;
        isetm_start[0] = ISETM_START;
        isetm_start[1] = ISETM_START;
        for (k=0;k<32;k++){
          if (arg.do_twiddle){
            // enable all
            tacbits_new[0][k] = 0x7;
            tacbits_new[1][k] = 0x7;
          }else{
            // disable all
            tacbits_new[0][k] = 0x0;
            tacbits_new[0][k] = 0x0;
          }
        }

        pt_printsend("Measuring GTVALID for crate %d, slot %d, TAC %d\n",
            arg.crate_num,i,wt);

        // loop over channel to measure inital GTVALID
        for (j=0;j<32;j++){
          if ((0x1<<j) & arg.pattern){
            // set pedestal enable for this channel
            xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,1<<j,&result,arg.crate_num,&thread_fdset);
            error = get_gtdelay_old(arg.crate_num,wt,&gt_temp0,&gt_temp1,isetm_start[(wt+1)%2],i,&thread_fdset);
            if (wt == 0)
              gtchan[j] = gt_temp0;
            else
              gtchan[j] = gt_temp1;
            gt_start[wt][j] = gtchan[j];
            if (error != 0){
              pt_printsend("Error getting gtdelay at slot %d, channel %d\n",i,j);
              unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
              return;
            }
          } // end if chan mask
        } // end loop over channels
        pt_printsend("\nMeasured initial GTValids\n");

        // find maximum gtvalid time
        gmax[wt] = 0.0;
        cmax[wt] = 0;
        for (j=0;j<32;j++)
          if ((0x1<<j) & arg.pattern)
            if (gtchan[j] > gmax[wt]){
              gmax[wt] = gtchan[j];
              cmax[wt] = j;
            }

        // find minimum gtvalid time
        gmin[wt] = 9999.0;
        cmin[wt] = 0;
        for (j=0;j<32;j++)
          if ((0x1<<j) & arg.pattern)
            if (gtchan[j] < gmin[wt]){
              gmin[wt] = gtchan[j];
              cmin[wt] = j;
            }


        // initial printout
        if (wt == 1){
          pt_printsend("GTVALID initial results, time in ns:\n");
          pt_printsend("---------------------------------------\n");
          pt_printsend("Crate Slot Chan GTDELAY 0/1:\n");
          for (j=0;j<32;j++){
            if ((0x1<<j) & arg.pattern)
              pt_printsend("%d %d %d %f %f\n",arg.crate_num,i,j,gt_start[0][j],gt_start[1][j]);
          }
          pt_printsend("TAC 0 max at chan %d: %f\n",cmax[0],gmax[0]);
          pt_printsend("TAC 1 max at chan %d: %f\n",cmax[1],gmax[1]);
          pt_printsend("TAC 0 min at chan %d: %f\n",cmin[0],gmin[0]);
          pt_printsend("TAC 1 min at chan %d: %f\n",cmin[1],gmin[1]);
        }

        // if gt_cutoff is set, we are going to change
        // the ISETM dacs untill all the channels are
        // just below it.
        if (arg.gt_cutoff != 0){
          pt_printsend("Finding ISETM values for crate %d, slot %d TAC %d\n",
              arg.crate_num,i,wt);
          isetm_new[wt] = ISETM;
          done = 0;

          while (!done){
            // load a new dac value
            error = loadsDac(d_isetm[wt],isetm_new[wt],arg.crate_num,i,&thread_fdset);
            if (error){
              pt_printsend("Error loading Dacs. Exiting\n");
              unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
              return;
            }
            // get a new measure of gtvalid
            xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<cmax[wt],&result,arg.crate_num,&thread_fdset);
            error = get_gtdelay(arg.crate_num,wt,&gt_temp0,&gt_temp1,isetm_new[(wt+1)%2],i,&thread_fdset);
            if (error != 0){
              pt_printsend("Error getting gtdelay at slot %d, channel %d\n",i,cmax[wt]);
              unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
              return;
            }
            if (wt == 0)
              gt_temp = gt_temp0;
            else
              gt_temp = gt_temp1;

            if (gt_temp <=  arg.gt_cutoff){
              done = 1;
            }else{
              if (isetm_new[wt] == 255){
                pt_printsend("warning - ISETM set to max!\n");
                done = 1;
              }else{
                isetm_new[wt]++;
              }
            }

          } // end while gt_temp > gt_cutoff 

          pt_printsend("Found ISETM value, checking new maximum\n");

          // check that we still have the max channel
          for (j=0;j<32;j++){
            if ((0x1<<j) & arg.pattern){
              xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<j,&result,arg.crate_num,&thread_fdset);
              error = get_gtdelay(arg.crate_num,wt,&gt_temp0,&gt_temp1,isetm_new[(wt+1)%2],i,&thread_fdset);
              if (error != 0){
                pt_printsend("Error getting gtdelay at slot %d, channel %d\n",i,j);
                unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
                return;
              }
              if (wt == 0)
                gtchan[j] = gt_temp0;
              else
                gtchan[j] = gt_temp1;
            }
          }
          // find maximum gtvalid time
          gt_max_sec = 0.0;
          chan_max_sec = 0;
          for (j=0;j<32;j++)
            if ((0x1<<j) & arg.pattern)
              if (gtchan[j] > gt_max_sec){
                gt_max_sec = gtchan[j];
                chan_max_sec = j;
              }


          // if the maximum channel has changed
          // refind the good isetm value
          if (chan_max_sec != cmax[wt]){
            pt_printsend("Warning, second chan_max not same as first.\n");
            cmax[wt] = chan_max_sec;
            gmax[wt] = gt_max_sec;
            done = 0;
            while (!done){
              error = loadsDac(d_isetm[wt],isetm_new[wt],arg.crate_num,i,&thread_fdset);
              xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<cmax[wt],&result,arg.crate_num,&thread_fdset);
              error = get_gtdelay(arg.crate_num,wt,&gt_temp0,&gt_temp1,isetm_new[(wt+1)%2],i,&thread_fdset);
              if (error != 0){
                pt_printsend("Error getting gtdelay at slot %d, channel %d\n",i,cmax[wt]);
                unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
                return;
              }
              if (wt == 0)
                gt_temp = gt_temp0;
              else
                gt_temp = gt_temp1;
              if (gt_temp <= arg.gt_cutoff)
                done = 1;
              else
                isetm_new[wt]++;
            }
          } // end if channel max changed

          // we are done, save the setup
          isetm_save[wt] = isetm_new[wt];
          for (j=0;j<32;j++)
            tacbits_save[wt][j] = tacbits_new[wt][j];

          // remeasure gtvalid
          for (j=0;j<32;j++)
            gtchan_set[wt][j] = 9999;
          for (j=0;j<32;j++){
            if ((0x1<<j) & arg.pattern){
              xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<j,&result,arg.crate_num,&thread_fdset);
              error = get_gtdelay(arg.crate_num,wt,&gt_temp0,&gt_temp1,isetm_new[(wt+1)%2],i,&thread_fdset);
              if (error != 0){
                pt_printsend("Error getting gtdelay at slot %d, channel %d\n",i,j);
                unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
                return;
              }
              if (wt == 0)
                gtchan_set[wt][j] = gt_temp0;
              else
                gtchan_set[wt][j] = gt_temp1;
            }
          }
          // find maximum
          gt_max_set[wt] = 0.0;
          chan_max_set[wt] = 0;
          for (j=0;j<32;j++)
            if ((0x1<<j) & arg.pattern)
              if (gtchan_set[wt][j] > gt_max_set[wt]){
                gt_max_set[wt] = gtchan_set[wt][j];
                chan_max_set[wt] = j;
              }
          // find minimum
          gt_min_set[wt] = 9999.0;
          chan_min_set[wt] = 0;
          for (j=0;j<32;j++)
            if ((0x1<<j) & arg.pattern)
              if (gtchan_set[wt][j] < gt_min_set[wt]){
                gt_min_set[wt] = gtchan_set[wt][j];
                chan_min_set[wt] = j;
              }

        } // end if gt_cutoff != 0

      } // end loop over tacs

      // reset pedestals
      xl3_rw(PED_ENABLE_R + select_reg + WRITE_REG,0x0,&result,arg.crate_num,&thread_fdset);

      // print out
      pt_printsend("\nCrate %d Slot %d - GTVALID FINAL results, time in ns:\n",arg.crate_num,i);
      pt_printsend("--------------------------------------------------------\n");
      if (!arg.do_twiddle)
        pt_printsend(" >>> ISETA0/1 = 0, no TAC twiddle bits set\n");
      pt_printsend("set up: VMAX: %hu, TACREF: %hu, ",VMAX,TACREF);
      pt_printsend("ISETA: %hu\n",ISETA);
      pt_printsend("Found ISETM0: %d, ISETM1: %d\n",isetm_save[0],isetm_save[1]);
      pt_printsend("Chan Tacbits GTValid 0/1:\n");
      for (j=0;j<32;j++){
        if ((0x1<<j) & arg.pattern){
          pt_printsend("%d 0x%hx %f %f",
              j,tacbits_save[1][j]*16 + tacbits_save[0][j],
              gtchan_set[0][j],gtchan_set[1][j]);
          if (isetm_save[0] == ISETM || isetm_save[1] == ISETM)
            pt_printsend(">>> Warning: isetm not adjusted\n");
          else
            pt_printsend("\n");
        }
      }

      pt_printsend(">>> Maximum TAC0 GTValid - Chan %d: %f\n",
          chan_max_set[0],gt_max_set[0]);
      pt_printsend(">>> Minimum TAC0 GTValid - Chan %d: %f\n",
          chan_min_set[0],gt_min_set[0]);
      pt_printsend(">>> Maximum TAC1 GTValid - Chan %d: %f\n",
          chan_max_set[1],gt_max_set[1]);
      pt_printsend(">>> Minimum TAC1 GTValid - Chan %d: %f\n",
          chan_min_set[1],gt_min_set[1]);

      if (abs(isetm_save[1] - isetm_save[0]) > 10)
        slot_errors |= 0x1;
      for (j=0;j<32;j++){
        if ((gtchan_set[0][j] < 0) || gtchan_set[1][j] < 0)
          chan_errors[j] = 1;
      }

      //store in DB
      if (arg.update_db){
        pt_printsend("updating the database\n");
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("cmos_m_gtvalid"));

        json_append_member(newdoc,"vmax",json_mknumber((double)VMAX));
        json_append_member(newdoc,"tacref",json_mknumber((double)TACREF));

        JsonNode* isetm_new = json_mkarray();
        JsonNode* iseta_new = json_mkarray();
        json_append_element(isetm_new,json_mknumber((double)isetm_save[0]));
        json_append_element(isetm_new,json_mknumber((double)isetm_save[1]));
        json_append_element(iseta_new,json_mknumber((double)ISETA));
        json_append_element(iseta_new,json_mknumber((double)ISETA));
        json_append_member(newdoc,"isetm",isetm_new);
        json_append_member(newdoc,"iseta",iseta_new);

        JsonNode* channels = json_mkarray();
        for (j=0;j<32;j++){
          JsonNode *one_chan = json_mkobject();
          json_append_member(one_chan,"id",json_mknumber((double) j));
          json_append_member(one_chan,"tac_shift",json_mknumber((double) (tacbits_save[1][j]*16+tacbits_save[0][1])));
          json_append_member(one_chan,"gtvalid0",json_mknumber((double) (gtchan_set[0][j])));
          json_append_member(one_chan,"gtvalid1",json_mknumber((double) (gtchan_set[1][j])));
          json_append_member(one_chan,"gtvalid0_old",json_mknumber((double) (gt_start[0][j])));
          json_append_member(one_chan,"gtvalid1_old",json_mknumber((double) (gt_start[0][j])));
          json_append_member(one_chan,"errors",json_mkbool(chan_errors[j]));
          if (chan_errors[j])
            slot_errors |= 0x2;
          json_append_element(channels,one_chan);
        }
        json_append_member(newdoc,"channels",channels);

        json_append_member(newdoc,"pass",json_mkbool(!(slot_errors)));
        json_append_member(newdoc,"slot_errors",json_mknumber(slot_errors));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	
        if (arg.ecal)
          json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
        json_delete(newdoc); // only delete the head
      }

    } // end if slot mask
  } // end loop over slot

  printsend("************************************************\n");

  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num);
}

int get_gtdelay_old(int crate_num, int wt, float *get_gttac0, float *get_gttac1, uint16_t isetm_orig, int slot, fd_set *thread_fdset)
{
  pt_printsend(".");
  fflush(stdout);


  float gtstart = 250;
  float gtdelay_even = gtstart;
  float gtdelay_odd = gtstart;
  float gtdelta_even = 10;
  float gtdelta_odd = 10;
  float reven = 1.0;
  float rodd = 1.0;
  int done_tac0 = 0;
  int done_tac1 = 0;
  int error = 0;
  uint32_t select_reg = FEC_SEL*slot;
  uint32_t result;
  uint32_t pmtword[3];
  int neven,nodd,nfire_even,nfire_odd,nfire,nfifo_old,nfifo;
  float set_delay;
  float gttemp0, gttemp1;

  // set unmeasured TAC GTVALID to long window time
  if (wt == 0){
    error += loadsDac(d_isetm[1],125,crate_num,slot,thread_fdset);
  }else{
    error += loadsDac(d_isetm[0],125,crate_num,slot,thread_fdset);
  }

  // find the time the TAC stops firing
  while ((done_tac0 == 0 && done_tac1 == 0)){
    // reset fifo
    xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result,crate_num,thread_fdset);
    xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result,crate_num,thread_fdset);
    usleep(500);

    // increment the gtdelay
    if (wt == 0)
      gtdelay_even += gtdelta_even;
    else
      gtdelay_odd += gtdelta_odd;

    // fire pedestals
    neven = 0;
    nodd = 0;
    nfire_even = 0;
    nfire_odd = 0;
    nfire = 0;
    nfifo_old = 0;
    int i;
    for (i=0;i<NGTVALID;i++){
      // set the gt delay, GTPED_DELAY corrects for disc pedestal delay
      // TEDLAY_EXTRA is extra correction to bring scope and target times
      // more inline with each other
      if (wt == 0){
        set_delay = gtdelay_even + GTPED_DELAY + TDELAY_EXTRA;
        gtdelay_even = set_gt_delay(set_delay) - GTPED_DELAY - TDELAY_EXTRA;
      }else{
        set_delay = gtdelay_odd + GTPED_DELAY + TDELAY_EXTRA;
        gtdelay_odd = set_gt_delay(set_delay) - GTPED_DELAY - TDELAY_EXTRA;
      }

      send_softgt();
      usleep(500);

      nfire++;

      xl3_rw(FIFO_WRITE_PTR_R + select_reg + READ_REG,0x0,&result,crate_num,thread_fdset);
      nfifo = (result & 0x000FFFFF)/3;
      if ((nfifo-nfifo_old) > 1){
        printf("Error in GT firing, stopping...\n");
        return -1;
      }

      if ((nfifo-nfifo_old) == 1){
        xl3_rw(select_reg + READ_MEM,0x0,pmtword,crate_num,thread_fdset);  
        xl3_rw(select_reg + READ_MEM,0x0,(pmtword+1),crate_num,thread_fdset);  
        xl3_rw(select_reg + READ_MEM,0x0,(pmtword+2),crate_num,thread_fdset);  

        if (UNPK_CELL_ID(pmtword)%2 == 0){
          neven++;
          nfire_even += nfire;
        }else{
          nodd++;
          nfire_odd++;
        }
        nfifo_old = nfifo;
        nfire = 0;
      }
    }

    // assign remainder of nfire
    if (nfire != NGTVALID && UNPK_CELL_ID(pmtword)%2 == 0){
      nfire_odd += nfire;
    }else{
      nfire_even += nfire;
    }

    // calculate ratio
    if (nfire_even != 0){
      reven = (float) neven / (float) (nfire_even);
    }else if (nfire_even == 0 && neven == 0){
      reven = 1;
    }else if (nfire_even == 0 && neven != 0){
      reven = 0;
    }
    if (nfire_odd != 0){
      rodd = (float) nodd / (float) (nfire_odd);
    }else if (nfire_even == 0 && nodd == 0){
      rodd = 1;
    }else if (nfire_even == 0 && nodd != 0){
      rodd = 0;
    }

    // check if we have gone out of bounds
    if (gtdelay_even > GTMAX) reven = 0;
    if (gtdelay_odd > GTMAX) rodd = 0;

    // check for solutions
    if (reven <= 0.5 && gtdelta_even == 1.0 && done_tac0 == 0){
      gttemp0 = gtdelay_even;
      *get_gttac0 = gttemp0;
      gtdelay_even -= 20;
      gtdelta_even = 0;
      done_tac0 = 1;
    }
    if (rodd <= 0.5 && gtdelta_odd == 1.0 && done_tac1 == 0){
      gttemp1 = gtdelay_odd;
      *get_gttac1 = gttemp1;
      gtdelay_odd -= 20;
      gtdelta_odd = 0;
      done_tac1 = 1;
    }

    // check if we are near a solution
    if (reven <= 0.5 && gtdelta_even == 10){
      reven = 1.0;
      gtdelta_even = 1.0;
      gtdelay_even -= 20.0;
    }
    if (rodd <= 0.5 && gtdelta_odd == 10){
      rodd = 1.0;
      gtdelta_odd = 1.0;
      gtdelay_odd -= 20.0;
    }
  }

  if (*get_gttac0 > GTMAX) *get_gttac0 = 999;
  if (*get_gttac1 > GTMAX) *get_gttac1 = 999;

  if (wt == 0)
    error += loadsDac(d_isetm[1],isetm_orig,crate_num,slot,thread_fdset);
  else
    error += loadsDac(d_isetm[0],isetm_orig,crate_num,slot,thread_fdset);

  return 0;
}
