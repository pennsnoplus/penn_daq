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
#include "mem_test.h"

int mem_test(char *buffer)
{
  mem_test_t *args;
  args = malloc(sizeof(mem_test_t));

  args->crate_num = 2;
  args->slot_num = 7;
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
          args->slot_num = atoi(words2);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        if ((words2 = strtok(NULL, " ")) != NULL)
          strcpy(args->ft_id,words2);
      }else if (words[1] == 'h'){
        printsend("Usage: mem_test -c [crate num (int)]"
            "-s [slot num (int)] -d (update database)\n");
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
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_mem_test,(void *)args);
  return 0; 
}

void *pt_mem_test(void *args)
{
  mem_test_t arg = *(mem_test_t *) args; 
  free(args);

  XL3_Packet packet;
  mem_test_args_t *packet_args = (mem_test_args_t *) packet.payload;
  mem_test_results_t *packet_results = (mem_test_results_t *) packet.payload;

  packet.cmdHeader.packet_type = MEM_TEST_ID;
  packet_args->slot_num = arg.slot_num;

  SwapLongBlock(packet_args,sizeof(mem_test_args_t)/sizeof(uint32_t));

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  pt_printsend("Getting crate configuration\n");
  update_crate_config(arg.crate_num,arg.slot_num,&thread_fdset);

  pt_printsend("Starting mem_test\n");

  do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);

  SwapLongBlock(packet_results,sizeof(mem_test_results_t)/sizeof(uint32_t));

  if (arg.update_db){
    pt_printsend("updating the database\n");
    char hextostr[50];
    int i;
    JsonNode *newdoc = json_mkobject();
    json_append_member(newdoc,"type",json_mkstring("mem_test"));

    // results of address test, which address bits are broken
    JsonNode* address_test = json_mkarray();
    for (i=0;i<20;i++){
      json_append_element(address_test,
          json_mkbool(!(packet_results->address_bit_failures & (0x1<<i))));
    }
    json_append_member(newdoc,"address_test",address_test);

    // results of pattern test, where first error was
    JsonNode* pattern_test = json_mkobject();
    json_append_member(pattern_test,"error",json_mkbool(packet_results->error_location!=0xFFFFFFFF));
    sprintf(hextostr,"%08x",packet_results->error_location);
    json_append_member(pattern_test,"error_location",json_mkstring(hextostr));
    sprintf(hextostr,"%08x",packet_results->expected_data);
    json_append_member(pattern_test,"expected_data",json_mkstring(hextostr));
    sprintf(hextostr,"%08x",packet_results->read_data);
    json_append_member(pattern_test,"read_data",json_mkstring(hextostr));
    json_append_member(newdoc,"pattern_test",pattern_test);

    json_append_member(newdoc,"pass",
        json_mkbool((packet_results->address_bit_failures == 0x0) &&
          (packet_results->error_location == 0xFFFFFFFF)));

    if (arg.final_test)
      json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_id));	

    post_debug_doc_mem_test(arg.crate_num,arg.slot_num,newdoc,&thread_fdset);
    json_delete(newdoc); // Only have to delete the head node
  }

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

