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
  args->use_debug = 0;

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


      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == 'D'){args->use_debug = 1;
      }else if (words[1] == 'E'){
        if ((words2 = strtok(NULL, " ")) != NULL){
          args->ecal = 1;
          strcpy(args->ecal_id,words2);
        }
      }else if (words[1] == 'h'){
        printsend("Usage: find_noise -c [crate mask (hex)] "
            "-s [slot mask for all crates (hex)] "
            "-00...-18 [slot mask for that crate (hex)] "
            "-D (use vthr zero values from debug db) "
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


  // get vthr zeros
  uint32_t *vthr_zeros = malloc(sizeof(uint32_t) * 10000);
  char get_db_address[500];
  if (arg.use_debug){
    // use zdisc debug values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        update_crate_config(i,arg.slot_mask[i],&thread_fdset);
        for (j=0;j<16;j++){
          if ((0x1<<j) & arg.slot_mask[j]){
            char config_string[500];
            sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
                crate_config[i][j].mb_id,crate_config[i][j].db_id[0],
                crate_config[i][j].db_id[1],crate_config[i][j].db_id[2],
                crate_config[i][j].db_id[3]);
            sprintf(get_db_address,"%s/%s/%s/get_zdisc?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
            printf("1- .%s.\n",get_db_address);
            pouch_request *zdisc_response = pr_init();
            pr_set_method(zdisc_response, GET);
            pr_set_url(zdisc_response, get_db_address);
            pr_do(zdisc_response);
            if (zdisc_response->httpresponse != 200){
              pt_printsend("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
              unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
              free(vthr_zeros);
              return;
            }
            JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
            JsonNode* viewrows = json_find_member(viewdoc,"rows");
            int n = json_get_num_mems(viewrows);
            if (n == 0){
              pt_printsend("Crate %d Slot %d: No zdisc documents for this configuration (%s). Exiting.\n",i,j,config_string);
              unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
              free(vthr_zeros);
              return;
            }
            JsonNode* zdisc_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode* zero_dac = json_find_member(zdisc_doc,"zero_dac");
            for (k=0;k<32;k++){
                vthr_zeros[i*32*16+j*32+k] = json_get_number(json_find_element(zero_dac,k));
            }
            json_delete(viewdoc); // only delete the head
            pr_free(zdisc_response);
          }
        }
      }
    }
  }else{
    // use the ECAL values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        for (j=0;j<16;j++){
          if ((0x1<<j) & arg.slot_mask[i]){
            sprintf(get_db_address,"%s/%s/%s/get_fec?startkey=[%d,%d,\"\"]&endkey=[%d,%d]&descending=true",FECDB_SERVER,FECDB_BASE_NAME,FECDB_VIEWDOC,i,j+1,i,j);
            printf("2- .%s.\n",get_db_address);
            pouch_request *fec_response = pr_init();
            pr_set_method(fec_response, GET);
            pr_set_url(fec_response, get_db_address);
            pr_do(fec_response);
            if (fec_response->httpresponse != 200){
              pt_printsend("Unable to connect to database. error code %d\n",(int)fec_response->httpresponse);
              unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
              free(vthr_zeros);
              return;
            }
            JsonNode *fecfull_doc = json_decode(fec_response->resp.data);
            JsonNode *fec_rows = json_find_member(fecfull_doc,"rows");
            int total_rows = json_get_num_mems(fec_rows); 
            if (total_rows == 0){
              pt_printsend("No FEC documents for this crate/card yet! (crate %d card %d)\n",i,j);
              unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
              free(vthr_zeros);
              return;
            }
            JsonNode *fecone_row = json_find_element(fec_rows,0);
            JsonNode *test_doc = json_find_member(fecone_row,"value");
            printf("doc is '%s'\n",json_encode(test_doc));
            JsonNode *hw = json_find_member(test_doc,"hw");
            JsonNode *zero_dac = json_find_member(hw,"vthr_zero");
            for (k=0;k<32;k++){
              vthr_zeros[i*32*16+j*32+k] = json_get_number(json_find_element(zero_dac,k));
            }

            json_delete(fecfull_doc);
            pr_free(fec_response);
          }
        }
      }
    }
  }



  uint32_t result;
  uint32_t dac_nums[50];
  uint32_t dac_values[50];
  for (i=0;i<32;i++){
    dac_nums[i] = d_vthr[i];
    dac_values[i] = 255;
  }

  uint32_t *base_noise;
  uint32_t *readout_noise;

  base_noise = malloc(sizeof(uint32_t) * 500000);
  readout_noise = malloc(sizeof(uint32_t) * 500000);

  // set up mtcd for pulsing continuously
  if (setup_pedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
        arg.crate_mask,arg.crate_mask)){
    pt_printsend("Error setting up mtc. Exiting\n");
    unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
    free(base_noise);
    free(readout_noise);
    free(vthr_zeros);
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
            free(vthr_zeros);
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
    printf("slot %d\n",j);
    int any_crates = 0;
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask)
        if ((0x1<<j) & arg.slot_mask[i])
	  any_crates = 1;
    }
    if (any_crates == 0)
      continue;
    // loop over channels
    for (k=0;k<32;k++){
      printf("chan %d\n",k);
      int threshabovezero = -2;
      uint32_t found_noise = 0x0;
      uint32_t done_mask = 0x0;
      for (i=0;i<19;i++){
        if (!((0x1<<j) & arg.slot_mask[i]))
          done_mask |= 0x1<<i;
      }

      // set pedestal masks
      for (i=0;i<19;i++)
        if ((0x1<<i) & arg.crate_mask)
          if ((0x1<<j) & arg.slot_mask[i])
            xl3_rw(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,(0x1<<k),&result,i,&thread_fdset);

      do{
        // send some peds
        multi_softgt(5000);

        for (i=0;i<19;i++){
          if (((0x1<<i) & arg.crate_mask) && ((0x1<<j) & arg.slot_mask[i]) && ((0x1<<i) & ~(done_mask))){
            // do the rest on XL3
            packet_args->slot_num = j;
            packet_args->chan_num = k;
            packet_args->thresh = (uint32_t) ((int)vthr_zeros[i*32*16+j*32+k]+threshabovezero);
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
  free(vthr_zeros);
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

  if (arg.update_db){
    pt_printsend("updating the database\n");
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        for (j=0;j<16;j++){
          if ((0x1<<j) & arg.slot_mask[i]){
            JsonNode *newdoc = json_mkobject();
            json_append_member(newdoc,"type",json_mkstring("find_noise"));
            JsonNode *channels = json_mkarray();
            for (k=0;k<32;k++){
              JsonNode *one_chan = json_mkobject();
              json_append_member(one_chan,"id",json_mknumber(k));
              json_append_member(one_chan,"zero_used",json_mknumber(vthr_zeros[i*32*16+j*32+k]));

              JsonNode *points = json_mkarray();
              int l;
              int finished = 0;
              for (l=0;l<33;l++){
                JsonNode *one_point = json_mkobject();
                json_append_member(one_point,"thresh_above_zero",json_mknumber(l-2));
                json_append_member(one_point,"base_noise",json_mknumber(base_noise[i*(16*32*33) + j*(32*33) + k*(33) + l]));
                json_append_member(one_point,"readout_noise",json_mknumber(readout_noise[i*(16*32*33) + j*(32*33) + k*(33) + l]));
                json_append_element(points,one_point);
                if (readout_noise[i*(16*32*33) + j*(32*33) + k*(33) + l] == 0){
                  if (finished)
                    break;
                  else
                    finished ++;
                }
              }
              json_append_member(one_chan,"points",points);

              json_append_element(channels,one_chan);
            }
            json_append_member(newdoc,"channels",channels);
            if (arg.ecal)
              json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
            json_append_member(newdoc,"pass",json_mkbool(1)); //FIXME
            post_debug_doc(i,j,newdoc,&thread_fdset);
            json_delete(newdoc); // Only have to delete the head node
          }
        }
      }
    }
  }


  //  disable_pulser();
  free(base_noise);
  free(readout_noise);
  free(vthr_zeros);
  unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
}

