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
#include "cald_test.h"

#define MAX_SAMPLES 10000

int cald_test(char *buffer)
{
  cald_test_t *args;
  args = malloc(sizeof(cald_test_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->upper = 3550;
  args->lower = 3000;
  args->num_points = 550;
  args->samples = 1;
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
      }else if (words[1] == 'u'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->upper = atoi(words2);
      }else if (words[1] == 'l'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->lower = atoi(words2);
      }else if (words[1] == 'n'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->num_points = atoi(words2);
      }else if (words[1] == 'p'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->samples = atoi(words2);
      }else if (words[1] == 'd'){args->update_db = 1;
      }else if (words[1] == '#'){
        args->final_test = 1;
        int i;
        for (i=0;i<16;i++)
          if ((0x1<<i) & args->slot_mask)
            if ((words2 = strtok(NULL, " ")) != NULL)
              strcpy(args->ft_ids[i],words2);
      }else if (words[1] == 'h'){
        printsend("Usage: cald_test -c [crate num (int)] "
            "-s [slot mask (hex)] -u [upper limit] -l [lower limit] "
            "-n [num points to sample] -p [samples per point] "
            "-d (update database)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  if (args->num_points*args->samples > MAX_SAMPLES){
    pt_printsend("too many points*samples! change malloc if you want more!\n");
    free(args);
    return 0;
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(0,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_cald_test,(void *)args);
  return 0; 
}


void *pt_cald_test(void *args)
{
  cald_test_t arg = *(cald_test_t *) args; 
  free(args);

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  pt_printsend("Starting cald test! Make sure you have inited the right xilinx!\n");

  uint16_t *point_buf;
  uint16_t *adc_buf;
  point_buf = malloc(16*MAX_SAMPLES*sizeof(uint16_t));
  adc_buf = malloc(16*4*MAX_SAMPLES*sizeof(uint16_t));
  if ((point_buf == NULL) || (adc_buf == NULL)){
    pt_printsend("Problem mallocing for cald test. Exiting\n");
    unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
    return;
  }

  int i,j;
  int num_slots = 0;
  for (i=0;i<16;i++)
    if ((0x1<<i) & arg.slot_mask)
      num_slots++;

  XL3_Packet packet;
  cald_test_args_t *packet_args = (cald_test_args_t *) packet.payload;

  packet.cmdHeader.packet_type = CALD_TEST_ID;
  packet_args->slot_mask = arg.slot_mask;
  packet_args->num_points = arg.num_points;
  packet_args->samples = arg.samples;
  packet_args->upper = arg.upper;
  packet_args->lower = arg.lower;
  SwapLongBlock(packet_args,sizeof(cald_test_args_t)/sizeof(uint32_t));
  do_xl3_cmd_no_response(&packet,arg.crate_num,&thread_fdset);

  int total_points = wait_for_cald_test_results(arg.crate_num,point_buf,adc_buf,&thread_fdset);
  pt_printsend("Got results of cald test. %d points received.\n",total_points);

  if (arg.update_db){
    pt_printsend("updating database\n");
    for (i=0;i<16;i++){
      if ((0x1<<i) & arg.slot_mask){
        JsonNode *newdoc = json_mkobject();
        json_append_member(newdoc,"type",json_mkstring("cald_test"));
        JsonNode *points = json_mkarray();
        JsonNode *adc0 = json_mkarray();
        JsonNode *adc1 = json_mkarray();
        JsonNode *adc2 = json_mkarray();
        JsonNode *adc3 = json_mkarray();
        int iter = 0;
        while(iter<=MAX_SAMPLES){
          if (iter != 0 && point_buf[i*MAX_SAMPLES+iter] == 0)
            break;
          pt_printsend("Slot %d - %u : %4u %4u %4u %4u\n",i,point_buf[i*MAX_SAMPLES+iter],adc_buf[i*4*MAX_SAMPLES+iter*4],adc_buf[i*4*MAX_SAMPLES+iter*4+1],adc_buf[i*4*MAX_SAMPLES+iter*4+2],adc_buf[i*4*MAX_SAMPLES+iter*4+3]);
          json_append_element(points,json_mknumber((double)point_buf[i*MAX_SAMPLES+iter]));
          json_append_element(adc0,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4]));
          json_append_element(adc1,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4+1]));
          json_append_element(adc2,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4+2]));
          json_append_element(adc3,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4+3]));
          iter++;
        }
        json_append_member(newdoc,"dac_value",points);
        json_append_member(newdoc,"adc_0",adc0);
        json_append_member(newdoc,"adc_1",adc1);
        json_append_member(newdoc,"adc_2",adc2);
        json_append_member(newdoc,"adc_3",adc3);
        json_append_member(newdoc,"pass",json_mkbool(1)); //FIXME
        if (arg.final_test)
          json_append_member(newdoc,"final_test_id",json_mkstring(arg.ft_ids[i]));	
        post_debug_doc(arg.crate_num,i,newdoc,&thread_fdset);
        json_delete(newdoc); // only delete the head node
      }
    }
  }

  free(point_buf);
  free(adc_buf);
  unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
}

