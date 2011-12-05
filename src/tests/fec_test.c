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
        printsend("Usage: fec_test -c [crate num (int)] "
            "-s [slot mask (hex)] -d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_fec_test,(void *)args);
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

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  /*
  MultiFC *commands = (MultiFC *) packet.payload;
  commands->howmany = 50;
  int i;
  for (i=0;i<25;i++){
    commands->cmd[i*2].address = 0x10F00023;
    commands->cmd[i*2+1].address = 0x00F00023;
    commands->cmd[i*2+1].data = i<<16;
  SwapLongBlock(&(commands->cmd[i*2].address),1);
  SwapLongBlock(&(commands->cmd[i*2+1].address),1);
  SwapLongBlock(&(commands->cmd[i*2+1].data),1);
  }
  SwapLongBlock(&(commands->howmany),1);

  packet.cmdHeader.packet_type = QUEUE_CMDS_ID;
  int errors = do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
  if (errors < 0){
    pt_printsend("Error queuing command\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }
  
  uint32_t result[50];
  errors = wait_for_multifc_results(50,command_number[arg.crate_num]-1,arg.crate_num,&result,&thread_fdset);
  if (errors < 0){
    pt_printsend("Error getting result\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  for (i=0;i<50;i++){
    pt_printsend("%d - %08x\n",i,result[i]);
  }


  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
  return;
//////////////////////////////////
*/
  do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);

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
        if (arg.ecal)
          json_append_member(newdoc,"ecal_id",json_mkstring(arg.ecal_id));	
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc); // Only have to delete the head node
      }
    }
  }

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

