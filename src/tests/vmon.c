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
#include "vmon.h"

int vmon(char *buffer)
{
  vmon_t *args;
  args = malloc(sizeof(vmon_t));

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
          args->slot_mask = strtoul(words2,(char **)NULL,16);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: vmon -c [crate num (int)]"
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
  pthread_create(new_thread,NULL,pt_vmon,(void *)args);
  return 0; 
}

void *pt_vmon(void *args)
{
  vmon_t arg = *(vmon_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  float voltages[16][21];
  int i,j;
  for (i=0;i<16;i++)
    for (j=0;j<21;j++)
      voltages[i][j] = 0;
  char v_name[21][20] = {"neg_24","neg_15","Vee","neg_3_3","neg_2","pos_3_3","pos_4","Vcc","pos_6_5","pos_8","pos_15","pos_24","neg_2_ref","neg_1_ref","pos_0_8_ref","pos_1_ref","pos_4_ref","pos_5_ref","Temp","CalD","hvt"};
  float voltages_min[21] = {-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99.,-99};
  float voltages_max[21] = {99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99.,99};


  XL3_Packet packet;
  vmon_args_t *packet_args = (vmon_args_t *) packet.payload;
  vmon_results_t *packet_results = (vmon_results_t *) packet.payload;
  int slot;
  for (slot=0;slot<16;slot++){
    if ((0x1<<slot) & arg.slot_mask){
      packet.cmdHeader.packet_type = VMON_ID;
      packet_args->slot_num = slot;
      SwapLongBlock(packet_args,sizeof(vmon_args_t)/sizeof(uint32_t));
      do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);

      SwapLongBlock(packet_results,sizeof(vmon_results_t)/sizeof(uint32_t));
      for (i=0;i<21;i++)
        voltages[slot][i] = packet_results->voltages[i];
    }
  }

  // now lets print out the results
  int k;
  for (k=0;k<2;k++){
    pt_printsend("slot             %2d     %2d     %2d     %2d     %2d     %2d     %2d     %2d\n",k*8,k*8+1,k*8+2,k*8+3,k*8+4,k*8+5,k*8+6,k*8+7);
    for (i=0;i<21;i++){
      pt_printsend("%10s   ",v_name[i]);
      for (j=0;j<8;j++){
        pt_printsend("%6.2f ",voltages[j+k*8][i]);
      }
      pt_printsend("\n");
    }
    pt_printsend("\n");
  }

  // update the database
  if (arg.update_db){
    pt_printsend("updating the database\n");
    char hextostr[50];
    for (slot=0;slot<16;slot++){
      if ((0x1<<slot) & arg.slot_mask){
        int pass_flag = 1;
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("vmon"));
        for (j=0;j<21;j++){
          json_append_member(newdoc,v_name[j],json_mknumber((double)voltages[slot][j]));
          if ((voltages[slot][j] < voltages_min[j]) || (voltages[slot][j] > voltages_max[j]))
            pass_flag = 0;
        }
        json_append_member(newdoc,"pass",json_mkbool(pass_flag));
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[slot]));	
        post_debug_doc(arg.crate_num,slot,newdoc,&thread_fdset);
        json_delete(newdoc);
      }
    }
  }

  xl3_lock[arg.crate_num] = 0;
  thread_done[arg.thread_num] = 1;
  return 0;
}

