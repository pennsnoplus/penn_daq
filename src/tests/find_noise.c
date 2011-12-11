#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"

#include "dac_number.h"
#include "xl3_registers.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "xl3_utils.h"
#include "mtc_utils.h"
#include "fec_utils.h"
#include "daq_utils.h"
#include "find_noise.h"

int find_noise(char *buffer)
{
  find_noise_t *args;
  args = malloc(sizeof(find_noise_t));

  args->crate_mask = 0xFFFFF;
  int i;
  for (i=0;i<19;i++)
    args->slot_mask[i] = 0xFFFF;
  args->update_db = 0;
  args->ecal = 0;

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


      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == 'E'){
        if ((words2 = strtok(NULL, " ")) != NULL){
          args->ecal = 1;
          strcpy(args->ecal_id,words2);
        }
      }else if (words[1] == 'h'){
        printsend("Usage: find_noise -c [crate mask (hex)] "
            "-s [slot mask for all crates (hex)] "
            "-00...-18 [slot mask for that crate (hex)] "
            "-d (update FEC database)\n");
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
  pthread_create(new_thread,NULL,pt_find_noise,(void *)args);
  return 0; 
}


void *pt_find_noise(void *args)
{
  find_noise_t arg = *(find_noise_t *) args; 
  free(args);

  int i,j,k;
  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  for (i=0;i<19;i++){
    if ((0x1<<i) & arg.crate_mask)
      FD_SET(rw_xl3_fd[i],&thread_fdset);
  }

  system("clear");
  pt_printsend("------------------------------------------\n");
  pt_printsend("Starting Noise Run\n");
  pt_printsend("------------------------------------------\n\n");
  pt_printsend("All crates and mtcs should have been inited with proper values already.\n\n");


  uint32_t result;
  uint32_t dac_nums[50];
  uint32_t dac_values[50];
  for (i=0;i<32;i++){
    dac_nums[i] = d_vthr[i];
    dac_values[i] = 255;
  }

  uint32_t *base_noise;
  uint32_t *readout_noise;
  uint32_t *threshold;

  base_noise = malloc(sizeof(uint32_t) * 500000);
  readout_noise = malloc(sizeof(uint32_t) * 500000);
  threshold = malloc(sizeof(uint32_t) * 500000);

  // set up mtcd for pulsing continuously
  if (setup_pedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
      arg.crate_mask,arg.crate_mask)){
    pt_printsend("Error setting up mtc. Exiting\n");
    unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
    free(base_noise);
    free(readout_noise);
    return;
  }

  // set all vthr dacs to 255 in all crates all slots
  for (i=0;i<19;i++)
    if ((0x1<<i) & arg.crate_mask)
      for (j=0;j<16;j++)
        if ((0x1<<j) & arg.slot_mask[i]){
          if (multi_loadsDac(32,dac_nums,dac_values,i,j,&thread_fdset) != 0){
            pt_printsend("Error loading dacs. Exiting\n");
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
            free(base_noise);
            free(readout_noise);
            return;
          }
        }
  

  // make sure fec memory is empty in all slots
  for (i=0;i<19;i++)
    if ((0x1<<i) & arg.crate_mask){
      XL3_Packet packet;
      packet.cmdHeader.packet_type = RESET_FIFOS_ID;
      reset_fifos_args_t *packet_args = (reset_fifos_args_t *) packet.payload;
      packet_args->slot_mask = arg.slot_mask[i];
      SwapLongBlock(packet_args,sizeof(reset_fifos_args_t)/sizeof(uint32_t));
      do_xl3_cmd(&packet,i,&thread_fdset);
      for (j=0;j<16;j++)
        if ((0x1<<j) & arg.slot_mask[i])
          xl3_rw(GENERAL_CSR_R + FEC_SEL*j + WRITE_REG,(i << FEC_CSR_CRATE_OFFSET),&result,i,&thread_fdset);
    }

  // going to do all crates simultaneously
  // first clear all pedestal masks
  for (i=0;i<19;i++)
    if ((0x1<<i) & arg.crate_mask)
      for (j=0;j<16;j++)
        if ((0x1<<j) & arg.slot_mask[j])
          xl3_rw(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,0x0,&result,i,&thread_fdset);


  XL3_Packet packet;
  packet.cmdHeader.packet_type = NOISE_TEST_ID;
  noise_test_args_t *packet_args = (noise_test_args_t *) packet.payload;
  noise_test_results_t *packet_results = (noise_test_results_t *) packet.payload;

  // loop over slots
  for (j=0;j<16;j++){
    // loop over channels
    for (k=0;k<32;k++){
      int chanzero = 115; //FIXME
      int threshabovezero = -2;
      uint32_t found_noise = 0x0;
      uint32_t done_mask = 0x0;

      // set pedestal masks
      for (i=0;i<19;i++)
        if ((0x1<<i) & arg.crate_mask)
          if ((0x1<<j) & arg.slot_mask[i])
            xl3_rw(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,(0x1<<k),&result,i,&thread_fdset);

      do{
        // send some peds
        multi_softgt(5000);

        for (i=0;i<19;i++){
          if (((0x1<<i) & arg.crate_mask) & ((0x1<<i) & !(done_mask))){
            // do the rest on XL3
            packet_args->slot_num = j;
            packet_args->chan_num = k;
            packet_args->chan_zero = chanzero;
            SwapLongBlock(packet_args,sizeof(noise_test_args_t)/sizeof(uint32_t));
            do_xl3_cmd(&packet,i,&thread_fdset);
            SwapLongBlock(packet_results,sizeof(noise_test_results_t)/sizeof(uint32_t));

            base_noise[i*(16*32*33)+j*(32*33)+k*(33)+threshabovezero+2] = packet_results->basenoise;
            readout_noise[i*(16*32*33)+j*(32*33)+k*(33)+threshabovezero+2] = packet_results->readoutnoise;

            if (packet_results->readoutnoise == 0){
              if ((0x1<<i) & found_noise)
                done_mask |= 0x1<<i;
              else
                found_noise |= 0x1<<i;
            }
          }
        }
        
        if (done_mask == arg.crate_mask)
          break;

      }while((++threshabovezero) <= 30);

      for (i=0;i<19;i++)
        if ((0x1<<i) & arg.crate_mask)
          loadsDac(d_vthr[k],255,i,j,&thread_fdset);

    } // end loop over channels
  } // end loop over slots


  /*

  // begin loop over all crates, all slots
  for (i=0;i<19;i++){
    if ((0x1<<i) & arg.crate_mask){
      for (j=0;j<16;j++){
        if ((0x1<<j) & arg.slot_mask[i]){

          // make sure threshold set to 255
          if (multi_loadsDac(32,dac_nums,dac_values,i,j,&thread_fdset) != 0){
            pt_printsend("Error loading dacs. Exiting\n");
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
            free(base_noise);
            free(readout_noise);
            return;
          }

          // clear pedestal mask
          xl3_rw(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,0x0,&result,i,&thread_fdset);

          // loop over channels
          for (k=0;k<32;k++){
            //FIXME if no known zero, skip it 
            int chanzero = 115; //FIXME


            int threshabovezero = -2;
            int found_noise = 0;

            XL3_Packet packet;
            packet.cmdHeader.packet_type = NOISE_TEST_ID;
            noise_test_args_t *packet_args = (noise_test_args_t *) packet.payload;
            noise_test_results_t *packet_results = (noise_test_results_t *) packet.payload;

            // enable pedestal for just that channel
            xl3_rw(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,(0x1<<k),&result,i,&thread_fdset);

            // find top of noise, loop until 30 counts above zero
            do{
              // send some peds
              multi_softgt(5000);

              // do the rest on XL3
              packet_args->slot_num = j;
              packet_args->chan_num = k;
              packet_args->chan_zero = chanzero+threshabovezero;
              SwapLongBlock(packet_args,sizeof(noise_test_args_t)/sizeof(uint32_t));
              do_xl3_cmd(&packet,i,&thread_fdset);
              SwapLongBlock(packet_results,sizeof(noise_test_results_t)/sizeof(uint32_t));

              if (packet_results->readoutnoise == 0){
                if (found_noise)
                  break;
                else
                  found_noise = 1;
              }

            }while((++threshabovezero) <= 50);

            loadsDac(d_vthr[k],255,i,j,&thread_fdset);

          } // end loop over channels



        } // if slot_mask
      } // end loop over slots
    } // if crate mask
  } // end loop over crates

  */

  //  disable_pulser();
  free(base_noise);
  free(readout_noise);
  unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
}

