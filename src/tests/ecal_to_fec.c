#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"

#include "db.h"
#include "xl3_rw.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "ecal_to_fec.h"

int ecal_to_fec(char *buffer)
{
  ecal_to_fec_t *args;
  args = malloc(sizeof(ecal_to_fec_t));

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == '#'){
        if ((words2 = strtok(NULL," ")) != NULL){
          strcpy(args->ecal_id,words2);
        }
      }else if (words[1] == 'h'){
        pt_printsend("Usage: ecal_to_fec -# [ecal id (hex)]\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_ecal_to_fec,(void *)args);
  return 0; 
}


void *pt_ecal_to_fec(void *args)
{
  ecal_to_fec_t arg = *(ecal_to_fec_t *) args; 
  free(args);

  int i,j;
  int result;

  char ecal_id[250];
  // now we can unlock our things, since nothing else should use them

  system("clear");
  pt_printsend("------------------------------------------\n");
  pt_printsend("Welcome to ECAL+-TO-FEC!\n");
  pt_printsend("------------------------------------------\n");
  pt_printsend("Using ecal id: %s\n",arg.ecal_id);

  // get the ecal document with the configuration
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,arg.ecal_id);
  pouch_request *ecaldoc_response = pr_init();
  pr_set_method(ecaldoc_response, GET);
  pr_set_url(ecaldoc_response, get_db_address);
  pr_do(ecaldoc_response);
  if (ecaldoc_response->httpresponse != 200){
    pt_printsend("Unable to connect to database. error code %d\n",(int)ecaldoc_response->httpresponse);
    unthread_and_unlock(0,0x0,arg.thread_num); 
    return;
  }
  JsonNode *ecalconfig_doc = json_decode(ecaldoc_response->resp.data);

  uint32_t crate_mask = 0x0;
  uint16_t slot_mask[20];
  for (i=0;i<20;i++){
    slot_mask[i] = 0x0;
  }

  // get the configuration
  JsonNode *crates = json_find_member(ecalconfig_doc,"crates");
  int num_crates = json_get_num_mems(crates);
  for (i=0;i<num_crates;i++){
    JsonNode *one_crate = json_find_element(crates,i);
    int crate_num = json_get_number(json_find_member(one_crate,"crate_id"));
    crate_mask |= (0x1<<crate_num);
    JsonNode *slots = json_find_member(one_crate,"slots");
    int num_slots = json_get_num_mems(slots);
    for (j=0;j<num_slots;j++){
      JsonNode *one_slot = json_find_element(slots,j);
      int slot_num = json_get_number(json_find_member(one_slot,"slot_id"));
      slot_mask[crate_num] |= (0x1<<slot_num);
    }
  }


  // get all the ecal test results for all crates/slots
  sprintf(get_db_address,"%s/%s/%s/get_ecal?startkey=\"%s\"&endkey=\"%s\"",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,arg.ecal_id,arg.ecal_id);
  pouch_request *ecal_response = pr_init();
  pr_set_method(ecal_response, GET);
  pr_set_url(ecal_response, get_db_address);
  pr_do(ecal_response);
  if (ecal_response->httpresponse != 200){
    pt_printsend("Unable to connect to database. error code %d\n",(int)ecal_response->httpresponse);
    unthread_and_unlock(0,0x0,arg.thread_num); 
    return;
  }

  JsonNode *ecalfull_doc = json_decode(ecal_response->resp.data);
  JsonNode *ecal_rows = json_find_member(ecalfull_doc,"rows");
  int total_rows = json_get_num_mems(ecal_rows); 
  if (total_rows == 0){
    pt_printsend("No documents for this ECAL yet! (id %s)\n",arg.ecal_id);
    unthread_and_unlock(0,0x0,arg.thread_num); 
    return;
  }

  // loop over crates/slots, create a fec document for each
  for (i=0;i<19;i++){
    if ((0x1<<i) & crate_mask){
      for (j=0;j<16;j++){
        if ((0x1<<j) & slot_mask[i]){
          printf("crate %d slot %d\n",i,j);

          // lets generate the fec document
          JsonNode *doc;
          create_fec_db_doc(i,j,&doc,ecalconfig_doc);

          int k;
          for (k=0;k<total_rows;k++){
            JsonNode *ecalone_row = json_find_element(ecal_rows,k);
            JsonNode *test_doc = json_find_member(ecalone_row,"value");
            JsonNode *config = json_find_member(test_doc,"config");
            if ((json_get_number(json_find_member(config,"crate_id")) == i) && (json_get_number(json_find_member(config,"slot")) == j)){
              printf("test type is %s\n",json_get_string(json_find_member(test_doc,"type")));
              add_ecal_test_results(doc,test_doc);
            }
          }

          post_fec_db_doc(i,j,doc);

          json_delete(doc); // only delete the head node
        }
      }
    }
  }

  json_delete(ecalfull_doc);
  pr_free(ecal_response);
  json_delete(ecalconfig_doc);
  pr_free(ecaldoc_response);

  pt_printsend("Finished creating fec document!\n");
  pt_printsend("**************************\n");

  unthread_and_unlock(0,0x0,arg.thread_num); 
}

