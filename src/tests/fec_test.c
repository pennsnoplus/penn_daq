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
#include "fec_test.h"

int fec_test(char *buffer)
{
  fec_test_t *args;
  args = malloc(sizeof(fec_test_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
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
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: fec_test -c [crate num (int)]"
            "-s [slot mask (hex)] -d (update database)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (xl3_lock[args->crate_num] == 0){
    // spawn a thread to do it
    xl3_lock[args->crate_num] = 1;

    pthread_t *new_thread;
    new_thread = malloc(sizeof(pthread_t));
    int i,thread_num = -1;
    for (i=0;i<MAX_THREADS;i++){
      if (thread_pool[i] == NULL){
        thread_pool[i] = new_thread;
        thread_num = i;
        break;
      }
    }
    if (thread_num == -1){
      printsend("All threads busy currently\n");
      free(args);
      return -1;
    }
    args->thread_num = thread_num;
    pthread_create(new_thread,NULL,pt_fec_test,(void *)args);
  }else{
    // this xl3 is locked, we cant do this right now
    free(args);
    return -1;
  }
  return 0; 
}


void *pt_fec_test(void *args)
{
  fec_test_t arg = *(fec_test_t *) args; 
  free(args);

  XL3_Packet packet;
  fec_test_args_t *packet_args = (fec_test_args_t *) packet.payload;
  fec_test_results_t *packet_results = (fec_test_results_t *) packet.payload;

  packet.cmdHeader.packet_type = FEC_TEST_ID;
  packet_args->slot_mask = arg.slot_mask;

  SwapLongBlock(packet.payload,1);

  do_xl3_cmd(&packet,arg.crate_num);

  SwapLongBlock(packet_results,sizeof(fec_test_results_t)/sizeof(uint32_t));

  if (arg.update_db){
    pt_printsend("updating the database\n");
    int slot,i;
    for (slot=0;slot<16;slot++){
      if ((0x1<<slot) & arg.slot_mask){
        pt_printsend("updating slot %d\n",slot);
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("fec_test"));
        json_append_member(newdoc,"pedestal",
            json_mkbool(!(packet_results->discrete_reg_errors[slot] & 0x1)));
        json_append_member(newdoc,"chip_disable",
            json_mkbool(!(packet_results->discrete_reg_errors[slot] & 0x2)));
        json_append_member(newdoc,"lgi_select",
            json_mkbool(!(packet_results->discrete_reg_errors[slot] & 0x4)));
        json_append_member(newdoc,"cmos_prog_low",
            json_mkbool(!(packet_results->discrete_reg_errors[slot] & 0x8)));
        json_append_member(newdoc,"cmos_prog_high",
            json_mkbool(!(packet_results->discrete_reg_errors[slot] & 0x10)));
        JsonNode *cmos_test_array = json_mkarray();
        for (i=0;i<32;i++){
          json_append_element(cmos_test_array,
              json_mkbool(!(packet_results->cmos_test_reg_errors[slot] & (0x1<<i))));
        }
        json_append_element(cmos_test_array,
            json_mkbool(packet_results->cmos_test_reg_errors[slot] == 0x0));
        json_append_member(newdoc,"cmos_test_reg",cmos_test_array);
        json_append_member(newdoc,"pass",
            json_mkbool((packet_results->discrete_reg_errors[slot] == 0x0) 
              && (packet_results->cmos_test_reg_errors[slot] == 0x0)));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        post_debug_doc(arg.crate_num,slot,newdoc);
        json_delete(newdoc); // Only have to delete the head node
      }
    }
  }

  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return 0;
}

