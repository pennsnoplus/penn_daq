#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "xl3_registers.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "xl3_utils.h"
#include "fec_utils.h"
#include "mtc_utils.h"
#include "run_pedestals.h"

int run_pedestals(char *buffer, int mtc, int crate)
{
  run_pedestals_t *args;
  args = malloc(sizeof(run_pedestals_t));

  args->mtc = mtc;
  args->crate = crate;
  args->crate_mask = 0x4;
  int i;
  for (i=0;i<19;i++)
    args->slot_mask[i] = 0xFFFF;
  args->pattern = 0xFFFFFFFF;
  args->frequency = 1000.0;
  args->gtdelay = DEFAULT_GT_DELAY;
  args->ped_width = DEFAULT_PED_WIDTH;

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
      }else if (words[1] == 'f'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->frequency = atof(words2);
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->pattern = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 't'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->gtdelay = atoi(words2);
      }else if (words[1] == 'w'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->ped_width = atoi(words2);
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
        }
      }else if (words[1] == 'h'){
        if (crate && mtc)
          printsend("Usage: run_pedestals ");
        else if (crate)
          printsend("Usage: run_pedestals_crate ");
        else
          printsend("Usage: run_pedestals_mtc ");
        if (crate)
          printsend("-c [crate mask (hex)] -s [slot mask for all crates (hex)] "
            "-00...-18 [slot mask for that crate (hex)] "
            "-p [channel mask (hex)] ");
        if (mtc)
          printsend("-f [pulser frequency Hz(float)] -w [ped width] -t [gt delay] ");
        printsend("\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  if (crate == 0)
    args->crate_mask = 0x0;
  pthread_t *new_thread;
  int thread_num = thread_and_lock(mtc,args->crate_mask,&new_thread);
  if (thread_num < 0){
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_run_pedestals,(void *)args);
  return 0; 
}

void *pt_run_pedestals(void *args)
{
  run_pedestals_t arg = *(run_pedestals_t *) args; 
  free(args);

  int i,errors;
  fd_set thread_fdset;
  // set up our fd_set for all the xl3s we've locked down
  if (arg.crate){
    FD_ZERO(&thread_fdset);
    for (i=0;i<19;i++)
      if (((0x1<<i) & arg.crate_mask) != 0x0)
        FD_SET(rw_xl3_fd[i],&thread_fdset);
  }

  if (arg.crate){
    pt_printsend("Running Pedestals - Crate Setup:\n");
    pt_printsend("--------------------------------\n");
    pt_printsend("Crate Mask:     0x%05x\n",arg.crate_mask);
    for (i=0;i<19;i++)
      if ((0x1<<i) & arg.crate_mask)
        pt_printsend("Crate %d: Slot Mask:    0x%04x\n",i,arg.slot_mask[i]); 
    pt_printsend("Channel Mask:     0x%08x\n",arg.pattern);
  }
  if (arg.mtc){
    pt_printsend("Running Pedestals - MTC Setup:\n");
    pt_printsend("--------------------------------\n");
    pt_printsend("GT delay (ns):      %3hu\n",arg.gtdelay);
    pt_printsend("Pedestal Width (ns):      %2d\n",arg.ped_width);
    pt_printsend("Pulser Frequency (Hz):      %3.0f\n",arg.frequency);
  }

  // Put all xl3s in init mode for now and reset all the FECs
  if (arg.crate){
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        pt_printsend("Preparing crate %d.\n",i);
        change_mode(INIT_MODE,0x0,i,&thread_fdset);
        int slot;
        for (slot=0;slot<16;slot++){
          if ((0x1<<slot) & arg.slot_mask[i]){
            uint32_t select_reg = FEC_SEL*slot;
            uint32_t result;
            xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result,i,&thread_fdset);
            xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result,i,&thread_fdset);
            xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result,i,&thread_fdset);
            xl3_rw(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result,i,&thread_fdset);
          }
        }
        deselect_fecs(i,&thread_fdset);
        errors = fec_load_crate_addr(i,arg.slot_mask[i],&thread_fdset);
        errors += set_crate_pedestals(i,arg.slot_mask[i],arg.pattern,&thread_fdset);
        deselect_fecs(i,&thread_fdset);
      }
    }
  }

  // now set up pedestals on mtc
  if (arg.mtc){
    errors = setup_pedestals(arg.frequency,arg.ped_width,arg.gtdelay,DEFAULT_GT_FINE_DELAY,
        arg.crate_mask | MSK_TUB, arg.crate_mask | MSK_TUB);
    if (errors != 0){
      pt_printsend("run_pedestals: Error setting up MTC. Exiting\n");
      unset_ped_crate_mask(MASKALL);
      unset_gt_crate_mask(MASKALL);
      unthread_and_unlock(arg.mtc,arg.crate_mask,arg.thread_num);
      return;
    }
  }

  // now enable the readout on the xl3s
  if (arg.crate)
    for (i=0;i<19;i++)
      if ((0x1<<i) & arg.crate_mask)
        change_mode(NORMAL_MODE,arg.slot_mask[i],i,&thread_fdset);

  unthread_and_unlock(arg.mtc,arg.crate_mask,arg.thread_num);
}

int run_pedestals_end(char *buffer, int mtc, int crate)
{
  run_pedestals_t *args;
  args = malloc(sizeof(run_pedestals_t));

  args->mtc = mtc;
  args->crate = crate;
  args->crate_mask = 0x4;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        if (crate && mtc)
          printsend("Usage: run_pedestals_end ");
        else if (crate)
          printsend("Usage: run_pedestals_end_crate ");
        else
          printsend("Usage: run_pedestals_end_mtc ");
        if (crate)
          printsend("-c [crate mask (hex)] ");
        printsend("\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  if (crate == 0)
    args->crate_mask = 0x0;
  pthread_t *new_thread;
  int thread_num = thread_and_lock(mtc,args->crate_mask,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_run_pedestals_end,(void *)args);
  return 0; 
}

void *pt_run_pedestals_end(void *args)
{
  run_pedestals_t arg = *(run_pedestals_t *) args; 
  free(args);

  int i;
  fd_set thread_fdset;
  // set up our fd_set for all the xl3s we've locked down
  if (arg.crate){
    FD_ZERO(&thread_fdset);
    for (i=0;i<19;i++)
      if (((0x1<<i) & arg.crate_mask) != 0x0)
        FD_SET(rw_xl3_fd[i],&thread_fdset);
  }


  // put all the crates back into init mode so they stop reading out
  // and turn off the pedestals
  if (arg.crate)
    for (i=0;i<19;i++)
      if ((0x1<<i) & arg.crate_mask){
        change_mode(INIT_MODE,arg.crate,0x0,&thread_fdset);
        set_crate_pedestals(i,0xFFFF,0x0,&thread_fdset);
      }

  // turn off the pulser and unmask all the crates
  if (arg.mtc){
    disable_pulser();
    disable_pedestal();
    unset_ped_crate_mask(MASKALL);
    unset_gt_crate_mask(MASKALL);
  }

  unthread_and_unlock(arg.mtc,arg.crate_mask,arg.thread_num);
}
