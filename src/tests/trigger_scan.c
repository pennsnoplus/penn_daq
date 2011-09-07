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
#include "mtc_utils.h"
#include "daq_utils.h"
#include "trigger_scan.h"

int trigger_scan(char *buffer)
{
  trigger_scan_t *args;
  args = malloc(sizeof(trigger_scan_t));

  args->trigger = 13;
  args->crate_mask = 0x4;
  args->min_thresh = 0;
  args->thresh_dac = -1;
  args->quick_mode = 0;
  args->total_nhit = -1;
  int i;
  for (i=0;i<19;i++)
    args->slot_mask[i] = 0xFFFF;
  sprintf(args->filename,"data/trigger_scan.dat");

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          for (i=0;i<19;i++)
            args->slot_mask[i] = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 't'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->trigger = atoi(words2);
      }else if (words[1] == 'f'){
        if ((words2 = strtok(NULL," ")) != NULL)
          sprintf(args->filename,"%s",words2);
      }else if (words[1] == 'n'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->total_nhit = atoi(words2);
      }else if (words[1] == 'm'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->min_thresh = atoi(words2);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->thresh_dac = atoi(words2);
      }else if (words[1] == 'n'){
          args->quick_mode = 1;
      }else if (words[1] == '0'){
        if (words[2] == '0'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[0] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[1] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '2'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[2] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '3'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[3] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '4'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[4] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '5'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[5] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '6'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[6] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '7'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[7] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '8'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[8] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '9'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[9] = strtoul(words2,(char**)NULL,16);
        }
      }else if (words[1] == '1'){
        if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[10] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[11] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '2'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[12] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '3'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[13] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '4'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[14] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '5'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[15] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '6'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[16] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '7'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[17] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '8'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[18] = strtoul(words2,(char**)NULL,16);
        }
      }else if (words[1] == 'h'){
        printsend("Usage: trigger_scan -c [crate mask (hex)] "
            "-t [trigger to enable (0-13)] -s [slot mask for all crates (hex)] "
            "-00..-18 [slot mask for crate 0-18 (hex)] -f [output file name] "
            "-n [max nhit to scan to (int)] -m [min adc count thresh to scan down to (int)] "
            "-d [threshold dac to program (by default the one you are triggering on)] "
            "-q (quick mode - samples every 10th dac count)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,args->crate_mask,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_trigger_scan,(void *)args);
  return 0; 
}


void *pt_trigger_scan(void *args)
{
  trigger_scan_t arg = *(trigger_scan_t *) args; 
  free(args);

  int i,j;
  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  for (i=0;i<19;i++)
    if ((0x1<<i) & arg.crate_mask)
      FD_SET(rw_xl3_fd[i],&thread_fdset);

  uint32_t select_reg,result,beforegt,aftergt;
  int num_fecs = 0;
  int min_nhit = 0;
  int last_zero, one_count,noise_count;
  float values[10000];
  uint32_t pedestals[19][16];
  uint16_t counts[14];
  for (i=0;i<14;i++)
    counts[i] = 10;
  FILE *file = fopen(arg.filename,"w");

  pt_printsend("Starting a trigger scan.\n");
  int errors = setup_pedestals(0, DEFAULT_PED_WIDTH, DEFAULT_GT_DELAY,0,
      MASKALL,MASKALL);
  //FIXME
  //    arg.crate_mask,arg.crate_mask);
  if (errors){
    pt_printsend("Error setting up MTC for pedestals. Exiting\n");
    unset_ped_crate_mask(MASKALL);
    unset_gt_crate_mask(MASKALL);
    unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
    return;
  }

  enable_pulser();
  enable_pedestal();

  for (i=0;i<19;i++)
    if ((0x1<<i) & arg.crate_mask)
      for (j=0;j<16;j++)
        if ((0x1<<j) & arg.slot_mask[i]){
          xl3_rw(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,0x0,&result,i,&thread_fdset);
          num_fecs++;
          pedestals[i][j] = 0x0;
        }

  // now we see our max number of nhit
  if (arg.total_nhit < 0){
    arg.total_nhit = num_fecs*32;
  }else if (arg.total_nhit > num_fecs*32){
    pt_printsend("You dont have enough fecs to test nhit that high!\n");
    arg.total_nhit = num_fecs*32;
    pt_printsend("Testing nhit up to %d\n",arg.total_nhit);
  }

  int ithresh,inhit,icrate,ifec;
  // we loop over threshold, coming down from max
  for (ithresh=0;ithresh<4095-arg.min_thresh;ithresh++){
    // if quick mode, do the first 50 then only every 10
    if (ithresh > 50 && arg.quick_mode == 1){ithresh += 9;}
    if (arg.thresh_dac >= 0)
      counts[arg.thresh_dac] = 4095-ithresh;
    else
      counts[arg.trigger-1] = 4095-ithresh;

    // now disable triggers while programming dacs
    // so we dont trigger off noise
    unset_gt_mask(0xFFFFFFFF);
    load_mtca_dacs_by_counts(counts);
    set_gt_mask(1<<(arg.trigger-1));
    
    for (i=0;i<10000;i++)
      values[i] = -1;
    last_zero = 0;
    noise_count = 0;
    one_count = 0;

    // now we loop over nhit
    // we loop over the small subset that interests us
    for (inhit=min_nhit;inhit<arg.total_nhit;inhit++){
      // we need to get our pedestals set up right
      // first we set up all the fully on fecs
      int full_fecs = inhit/32;
      int unfull_fec = inhit%32;
      uint32_t unfull_pedestal = 0x0;
      for (i=0;i<unfull_fec;i++)
        unfull_pedestal |= 0x1<<i;

      for (icrate=0;icrate<19;icrate++){
        if ((0x1<<icrate) & arg.crate_mask){
          for (ifec=0;ifec<16;ifec++){
            if ((0x1<<ifec) & arg.slot_mask[icrate]){
              if (pedestals[icrate][ifec] == 0xFFFFFFFF){
                if (full_fecs > 0){
                  // we should keep this one fully on
                  full_fecs--;
                }else if (full_fecs == 0){
                 // this one gets the leftovers
                  full_fecs--;
                  pedestals[icrate][ifec] = unfull_pedestal;
                  xl3_rw(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],
                      &result,icrate,&thread_fdset);
                }else{
                  // turn this one back off
                  pedestals[icrate][ifec] = 0x0;
                  xl3_rw(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],
                      &result,icrate,&thread_fdset);
                }
              }else{
                if (full_fecs > 0){
                  // turn this one back fully on
                  full_fecs--;
                  pedestals[icrate][ifec] = 0xFFFFFFFF;
                  xl3_rw(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],
                      &result,icrate,&thread_fdset);
                }else if (full_fecs == 0){
                  // this one should get the leftovers
                  full_fecs--;
                  pedestals[icrate][ifec] = unfull_pedestal;
                  xl3_rw(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],
                      &result,icrate,&thread_fdset);
                }else if (pedestals[icrate][ifec] == 0x0){
                  // this one can stay off
                }else{
                  // turn this one fully off
                  pedestals[icrate][ifec] = 0x0;
                  xl3_rw(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],
                      &result,icrate,&thread_fdset);
                }
              }
            } // end if in slot mask
          } // end loop over fecs
        } // end if in crate mask
      } // end loop over crates

      // we are now sitting at the correct nhit
      // and we have the right threshold set
      // lets do this trigger check thinger

      // get initial gt count
      mtc_reg_read(MTCOcGtReg,&beforegt);

      // send 500 pulses
      multi_softgt(500);

      // now get final gt count
      mtc_reg_read(MTCOcGtReg,&aftergt);

      // top bits are junk
      uint32_t diff = (aftergt & 0x00FFFFFF) - (beforegt & 0x00FFFFFF);
      values[inhit] = (float) diff / 500.0;

      // we will start at an nhit based on where
      // we start seeing triggers
      if (values[inhit] == 0)
        last_zero = inhit;

      // we will stop at an nhit based on where
      // we hit the triangle of bliss
      if (values[inhit] > 0.9 && values[inhit] < 1.1)
        one_count++;

      // we will also stop if we are stuck in the
      // noise for too long meaning we arent at a
      // high enough threshold to see the triangle
      // of bliss
      if (values[inhit] > 1.2)
        noise_count++;

      if (one_count > 5 || noise_count > 25){
        // we are done with this threshold
        min_nhit = last_zero < 5 ? 0 : last_zero-5;
        break;
      }

    } // end loop over nhit

    // now write out this thresholds worth of results
    // to file
    for (i=0;i<10000;i++){
      // only print out nhit we tested
      if (values[i] >= 0){
        fprintf(file,"%d %d %f\n",ithresh,i,values[i]);
      }
    }
    pt_printsend("Finished %d\n",ithresh);
  } // end loop over thresh

  unset_gt_mask(MASKALL);
  fclose(file);
  pt_printsend("Finished trigger scan!\n");

  unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
  return;
}

