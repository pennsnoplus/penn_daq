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
#include "zdisc.h"

int zdisc(char *buffer)
{
  zdisc_t *args;
  args = malloc(sizeof(zdisc_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->rate = 100;
  args->offset = 0;
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
      }else if (words[1] == 'r'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->rate = atof(words2);
      }else if (words[1] == 'o'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->offset = atoi(words2);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: zdisc -c [crate num (int)] "
            "-s [slot mask (hex)] -o [offset] -r [rate] "
            "-d (update database)\n");
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
  pthread_create(new_thread,NULL,pt_zdisc,(void *)args);
  return 0; 
}


void *pt_zdisc(void *args)
{
  zdisc_t arg = *(zdisc_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  pt_printsend("------------------------------------\n");
  pt_printsend("Discriminator Zero Finder.\n");
  pt_printsend("Desired rate:\t 5.1f\n",arg.rate);
  pt_printsend("Offset:\t %hu\n",arg.offset);
  pt_printsend("------------------------------------\n");

  XL3_Packet packet;
  zdisc_args_t *packet_args = (zdisc_args_t *) packet.payload;
  zdisc_results_t *packet_results = (zdisc_results_t *) packet.payload;

  int i,j;
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      // tell xl3 to zdisc
      packet.cmdHeader.packet_type = ZDISC_ID;
      packet_args->slot_num = i;
      packet_args->offset = arg.offset;
      packet_args->rate = arg.rate;
      SwapLongBlock(packet_args,sizeof(zdisc_args_t)/sizeof(uint32_t));
      do_xl3_cmd(&packet,arg.crate_num,&thread_fdset);
      SwapLongBlock(packet_results,97); // not all is uint32_ts

      // printout
      pt_printsend("channel    max rate,       lower,       upper\n");
      pt_printsend("------------------------------------------\n");
      for (j=0;j<32;j++)
        pt_printsend("ch (%2d)   %5.2f(MHz)  %6.1f(KHz)  %6.1f(KHz)\n",
            j,packet_results->MaxRate[j]/1E6,packet_results->LowerRate[j]/1E3,
            packet_results->UpperRate[j]/1E3);
      pt_printsend("Dac Settings\n");
      pt_printsend("channel     Max   Lower   Upper   U+L/2\n");
      for (j=0;j<32;j++)
      {
        pt_printsend("ch (%2i)   %5hu   %5hu   %5hu   %5hu\n",
            j,packet_results->MaxDacSetting[j],packet_results->LowerDacSetting[j],
            packet_results->UpperDacSetting[j],packet_results->ZeroDacSetting[j]);
        if (packet_results->LowerDacSetting[j] > packet_results->MaxDacSetting[j])
          pt_printsend(" <- lower > max! (MaxRate(MHz):%5.2f, lowrate(KHz):%5.2f\n",
              packet_results->MaxRate[j]/1E6,packet_results->LowerRate[j]/1E3);
      }

      // update the database
      if (arg.update_db){
        pt_printsend("updating the database\n");
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("zdisc"));

        JsonNode *maxratenode = json_mkarray();
        JsonNode *lowerratenode = json_mkarray();
        JsonNode *upperratenode = json_mkarray();
        JsonNode *maxdacnode = json_mkarray();
        JsonNode *lowerdacnode = json_mkarray();
        JsonNode *upperdacnode = json_mkarray();
        JsonNode *zerodacnode = json_mkarray();
        JsonNode *errorsnode = json_mkarray();
        for (j=0;j<32;j++){
          json_append_element(maxratenode,json_mknumber(packet_results->MaxRate[j]));	
          json_append_element(lowerratenode,json_mknumber(packet_results->LowerRate[j]));	
          json_append_element(upperratenode,json_mknumber(packet_results->UpperRate[j]));	
          json_append_element(maxdacnode,json_mknumber((double)packet_results->MaxDacSetting[j]));	
          json_append_element(lowerdacnode,json_mknumber((double)packet_results->LowerDacSetting[j]));	
          json_append_element(upperdacnode,json_mknumber((double)packet_results->UpperDacSetting[j]));	
          json_append_element(zerodacnode,json_mknumber((double)packet_results->ZeroDacSetting[j]));	
          json_append_element(errorsnode,json_mkbool(0));//FIXME	
        }
        json_append_member(newdoc,"max_rate",maxratenode);
        json_append_member(newdoc,"lower_rate",lowerratenode);
        json_append_member(newdoc,"upper_rate",upperratenode);
        json_append_member(newdoc,"max_dac",maxdacnode);
        json_append_member(newdoc,"lower_dac",lowerdacnode);
        json_append_member(newdoc,"upper_dac",upperdacnode);
        json_append_member(newdoc,"zero_dac",zerodacnode);
        json_append_member(newdoc,"errors",errorsnode);
        json_append_member(newdoc,"pass",json_mkbool(1));//FIXME
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	

        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
        json_delete(newdoc); // Only need to delete the head node);
      }


    } // end if in slot mask
  } // end loop over slots

  pt_printsend("Zero Discriminator complete.\n");
  pt_printsend("*********************************\n");

  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

