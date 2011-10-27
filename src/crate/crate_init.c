#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"

#include "db.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "crate_init.h"

int crate_init(char *buffer)
{
  crate_init_t *args;
  args = malloc(sizeof(crate_init_t));

  args->crate_num = 2;
  args->xilinx_load = 0;
  args->hv_reset = 0;
  args->shift_reg_only = 0;
  args->slot_mask = 0x80;
  args->use_cbal = 0;
  args->use_zdisc = 0;
  args->use_ttot = 0;
  args->use_all = 0;
  args->use_hw = 0;

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
      }else if (words[1] == 'x'){args->xilinx_load = 1;
      }else if (words[1] == 'X'){args->xilinx_load = 2;
      }else if (words[1] == 'v'){args->hv_reset = 1;
      }else if (words[1] == 'b'){args->use_cbal = 1;
      }else if (words[1] == 'd'){args->use_zdisc = 1;
      }else if (words[1] == 't'){args->use_ttot = 1;
      }else if (words[1] == 'a'){args->use_all = 1;
      }else if (words[1] == 'w'){args->use_hw = 1;
      }else if (words[1] == 'h'){
        printsend("Usage: crate_init -c [crate num (int)]"
            "-s [slot mask (hex)] -x (load xilinx) -X (load cald xilinx)"
            "-v (reset HV dac) -b (load cbal from db) -d (load zdisc from db)"
            "-t (load ttot from db) -a (load all from db)"
            "-w (use crate/card specific values from db)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return thread_num;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_crate_init,(void *)args);
  return 0; 
}

void *pt_crate_init(void *args)
{
  crate_init_t arg = *(crate_init_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  char get_db_address[500];
  char ctc_address[500];
  XL3_Packet packet;
  crate_init_args_t *packet_args = (crate_init_args_t *) packet.payload;
  crate_init_results_t *packet_results = (crate_init_results_t *) packet.payload;;

  pt_printsend("Initializing crate %d, slots %08x, xl: %d, hv: %d\n",
      arg.crate_num,arg.slot_mask,arg.xilinx_load,arg.hv_reset);
  pt_printsend("Now sending database to XL3\n");

  pouch_request *hw_response = pr_init();
  JsonNode* hw_rows = NULL;
  pouch_request *debug_response = pr_init();
  JsonNode* debug_doc = NULL;

  if (arg.use_hw == 1){
    sprintf(get_db_address,"%s/%s/%s/get_fec?startkey=[%d,0]&endkey=[%d,15]",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,arg.crate_num,arg.crate_num);
    pr_set_method(hw_response, GET);
    pr_set_url(hw_response, get_db_address);
    pr_do(hw_response);
    if (hw_response->httpresponse != 200){
      pt_printsend("Unable to connect to database. error code %d\n",(int)hw_response->httpresponse);
      unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
      return;
    }
    JsonNode *hw_doc = json_decode(hw_response->resp.data);
    JsonNode* totalrows = json_find_member(hw_doc,"total_rows");
    if ((int)json_get_number(totalrows) != 16){
      pt_printsend("Database error: not enough FEC entries\n");
      unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
      return;
    }
    hw_rows = json_find_member(hw_doc,"rows");
    //json_delete(hw_doc); // only delete the head node
    pr_free(hw_response);
  }else{
    sprintf(get_db_address,"%s/%s/CRATE_INIT_DOC",DB_SERVER,DB_BASE_NAME);
    pr_set_method(debug_response, GET);
    pr_set_url(debug_response, get_db_address);
    pr_do(debug_response);
    if (debug_response->httpresponse != 200){
      pt_printsend("Unable to connect to database. error code %d\n",(int)debug_response->httpresponse);
      unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
      return;
    }
    debug_doc = json_decode(debug_response->resp.data);
    pr_free(debug_response);
  }

  // make sure crate_config is up to date
  if (arg.use_cbal || arg.use_zdisc || arg.use_ttot || arg.use_all)
    update_crate_config(arg.crate_num,arg.slot_mask,&thread_fdset);

  // GET ALL FEC DATA FROM DB
  int i,j,crate,card;
  for (i=0;i<16;i++){

    mb_t* mb_consts = (mb_t *) (packet.payload+4);
    packet.cmdHeader.packet_type = CRATE_INIT_ID;

    ///////////////////////////
    // GET DEFAULT DB VALUES //
    ///////////////////////////

    if (arg.use_hw == 1){
      JsonNode* next_row = json_find_element(hw_rows,i);
      JsonNode* key = json_find_member(next_row,"key");
      JsonNode* value = json_find_member(next_row,"value");
      JsonNode* hw = json_find_member(value,"hw");
      crate = (int)json_get_number(json_find_element(key,0));
      card = (int)json_get_number(json_find_element(key,1));
      if (crate != arg.crate_num || card != i){
        printsend("Database error : incorrect crate or card num (%d,%d)\n",crate,card);
        unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
        return;
      }
      parse_fec_hw(value,mb_consts);
    }else{
      parse_fec_debug(debug_doc,mb_consts);
    }

    //////////////////////////////
    // GET VALUES FROM DEBUG DB //
    //////////////////////////////

    if ((arg.use_cbal || arg.use_all) && ((0x1<<i) & arg.slot_mask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
        printsend("Warning: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n");
      }else{
        char config_string[500];
        sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
            crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].dc_id[0],
            crate_config[arg.crate_num][i].dc_id[1],crate_config[arg.crate_num][i].dc_id[2],
            crate_config[arg.crate_num][i].dc_id[3]);
        sprintf(get_db_address,"%s/%s/%s/get_crate_cbal?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
            DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
        pouch_request *cbal_response = pr_init();
        pr_set_method(cbal_response, GET);
        pr_set_url(cbal_response, get_db_address);
        pr_do(cbal_response);
        if (cbal_response->httpresponse != 200){
          pt_printsend("Unable to connect to database. error code %d\n",(int)cbal_response->httpresponse);
          unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        JsonNode *viewdoc = json_decode(cbal_response->resp.data);
        JsonNode* viewrows = json_find_member(viewdoc,"rows");
        int n = json_get_num_mems(viewrows);
        if (n == 0){
          pt_printsend("No crate_cbal documents for this configuration (%s). Continuing with default values.\n",config_string);
        }else{
          // these next three JSON nodes are pointers to the structure of viewrows; no need to delete
          JsonNode* cbal_doc = json_find_member(json_find_element(viewrows,0),"value");
          JsonNode* channels = json_find_member(cbal_doc,"channels");
          for (j=0;j<32;j++){
            JsonNode* one_channel = json_find_element(channels,j); 
            mb_consts->vbal[1][j] = (int)json_get_number(json_find_member(one_channel,"vbal_low"));
            mb_consts->vbal[0][j] = (int)json_get_number(json_find_member(one_channel,"vbal_high"));
          }
        }
        json_delete(viewdoc); // viewrows is part of viewdoc; only delete the head node
        pr_free(cbal_response);
      }
    }

    if ((arg.use_zdisc || arg.use_all) && ((0x1<<i) & arg.slot_mask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
      }else{
        char config_string[500];
        sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
            crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].dc_id[0],
            crate_config[arg.crate_num][i].dc_id[1],crate_config[arg.crate_num][i].dc_id[2],
            crate_config[arg.crate_num][i].dc_id[3]);
        sprintf(get_db_address,"%s/%s/%s/get_zdisc?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
        pouch_request *zdisc_response = pr_init();
        pr_set_method(zdisc_response, GET);
        pr_set_url(zdisc_response, get_db_address);
        pr_do(zdisc_response);
        if (zdisc_response->httpresponse != 200){
          pt_printsend("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
          unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
        JsonNode* viewrows = json_find_member(viewdoc,"rows");
        int n = json_get_num_mems(viewrows);
        if (n == 0){
          pt_printsend("No zdisc documents for this configuration (%s). Continuing with default values.\n",config_string);
        }else{
          JsonNode* zdisc_doc = json_find_member(json_find_element(viewrows,0),"value");
          JsonNode* vthr = json_find_member(zdisc_doc,"zero_dac");
          for (j=0;j<32;j++){
            mb_consts->vthr[j] = (int)json_get_number(json_find_element(vthr,j));
          }
        }
        json_delete(viewdoc); // only delete the head
        pr_free(zdisc_response);
      }
    }


    if ((arg.use_ttot || arg.use_all) && ((0x1<<i) & arg.slot_mask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
        pt_printsend("Warning: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n");
      }else{
        char config_string[500];
        sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
            crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].dc_id[0],
            crate_config[arg.crate_num][i].dc_id[1],crate_config[arg.crate_num][i].dc_id[2],
            crate_config[arg.crate_num][i].dc_id[3]);
        sprintf(get_db_address,"%s/%s/%s/get_ttot?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
            DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
        pouch_request *ttot_response = pr_init();
        pr_set_method(ttot_response, GET);
        pr_set_url(ttot_response, get_db_address);
        pr_do(ttot_response);
        if (ttot_response->httpresponse != 200){
          pt_printsend("Unable to connect to database. error code %d\n",(int)ttot_response->httpresponse);
          unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
          return;
        }
        JsonNode *viewdoc = json_decode(ttot_response->resp.data);
        JsonNode* viewrows = json_find_member(viewdoc,"rows");
        int n = json_get_num_mems(viewrows);
        if (n == 0){
          pt_printsend("No set_ttot documents for this configuration (%s). Continuing with default values.\n",config_string);
        }else{
          JsonNode* ttot_doc = json_find_member(json_find_element(viewrows,0),"value");
          JsonNode* chips = json_find_member(ttot_doc,"chips");
          for (j=0;j<8;j++){
            JsonNode* one_chip = json_find_element(chips,j);
            mb_consts->tdisc.rmp[j] = (int)json_get_number(json_find_member(one_chip,"rmp"));
            mb_consts->tdisc.vsi[j] = (int)json_get_number(json_find_member(one_chip,"vsi"));
          }
        }
        json_delete(viewdoc);
        pr_free(ttot_response);
      }
    }


    ///////////////////////////////
    // SEND THE DATABASE TO XL3s //
    ///////////////////////////////

    packet_args->mb_num = i;

    SwapLongBlock(&(packet.payload),1);	
    swap_fec_db(mb_consts);
    do_xl3_cmd_no_response(&packet,arg.crate_num);

  }

  // GET CTC DELAY FROM CTC_DOC IN DB
  pouch_request *ctc_response = pr_init();
  sprintf(ctc_address,"%s/%s/CTC_doc",DB_SERVER,DB_BASE_NAME);
  pr_set_method(ctc_response, GET);
  pr_set_url(ctc_response, ctc_address);
  pr_do(ctc_response);
  if (ctc_response->httpresponse != 200){
    pt_printsend("Error getting ctc document, error code %d\n",(int)ctc_response->httpresponse);
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }
  JsonNode *ctc_doc = json_decode(ctc_response->resp.data);
  JsonNode *ctc_delay_a = json_find_member(ctc_doc,"delay");
  uint32_t ctc_delay = strtoul(json_get_string(json_find_element(ctc_delay_a,arg.crate_num)),(char**) NULL,16);
  json_delete(ctc_doc); // delete the head node
  pr_free(ctc_response);


  // START CRATE_INIT ON ML403
  pt_printsend("Beginning crate_init.\n");

  packet.cmdHeader.packet_type = CRATE_INIT_ID;
  packet_args->mb_num = 666;
  packet_args->xilinx_load = arg.xilinx_load;
  packet_args->hv_reset = arg.hv_reset;
  packet_args->slot_mask = arg.slot_mask;
  packet_args->ctc_delay = ctc_delay;
  packet_args->shift_reg_only = arg.shift_reg_only;

  SwapLongBlock(&(packet.payload),sizeof(crate_init_args_t)/sizeof(uint32_t));

  do_xl3_cmd(&packet,arg.crate_num,&thread_fdset); 

  // NOW PROCESS RESULTS AND POST TO DB
  for (i=0;i<16;i++){
    crate_config[arg.crate_num][i] = packet_results->hware_vals[i];
    SwapShortBlock(&(crate_config[arg.crate_num][i].mb_id),1);
    SwapShortBlock(&(crate_config[arg.crate_num][i].dc_id),4);
  }

  
  pt_printsend("Crate configuration updated.\n");
  pt_printsend("*******************************\n");
  json_delete(hw_rows);
  json_delete(debug_doc);


  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}
