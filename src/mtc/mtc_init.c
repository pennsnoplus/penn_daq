#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pouch.h"
#include "json.h"

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_utils.h"
#include "mtc_init.h"

int mtc_init(char *buffer)
{
  int xilinx_load = 0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'x'){
        xilinx_load = 1;
      }else if (words[1] == 'h'){
        printsend("Usage: mtc_init -x (load xilinx)\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  // now check and see if everything needed is unlocked
  if (sbc_lock != 0){
    // the sbc is locked, we cant do this right now
    printf("we cant, its locked!\n");
    return -1;
  }
  if (sbc_connected == 0){
    printsend("SBC is not connected! Aborting\n");
    return 0;
  }
  // spawn a thread to do it
  sbc_lock = 1;

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
    return -1;
  }

  uint32_t *args;
  args = malloc(3*sizeof(uint32_t));
  args[0] = (uint32_t) thread_num;
  args[1] = (uint32_t) xilinx_load;
  pthread_create(new_thread,NULL,pt_mtc_init,(void *)args);
  return 0; 
}

void *pt_mtc_init(void *args)
{
  int thread_num = ((uint32_t *) args)[0];
  int xilinx_load = ((uint32_t *) args)[1];
  free(args);

  if (xilinx_load)
    mtc_xilinx_load();

  mtc_t *mtc = ( mtc_t * ) malloc( sizeof(mtc_t));
  if ( mtc == ( mtc_t *) NULL )
  {
    pt_printsend("error: malloc in mtc_init\n");
    free(mtc);
    sbc_lock = 0;
    thread_done[thread_num] = 1;
    return;
  }

  pouch_request *response = pr_init();
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/MTC_doc",DB_SERVER,DB_BASE_NAME);
  pr_set_method(response, GET);
  pr_set_url(response, get_db_address);
  pr_do(response);
  if (response->httpresponse != 200){
    pt_printsend("Unable to connect to database. error code %d\n",(int)response->httpresponse);
    sbc_lock = 0;
    thread_done[thread_num] = 1;
    return;
  }
  JsonNode *doc = json_decode(response->resp.data);
  parse_mtc(doc,mtc);
  json_delete(doc);

  /*
  //unset all masks
  unset_gt_mask(MASKALL);
  unset_ped_crate_mask(MASKALL);
  unset_gt_crate_mask(MASKALL);

  // load the dacs
  mtc_cons *mtc_cons_ptr = ( mtc_cons * ) malloc( sizeof(mtc_cons));
  if ( mtc_cons_ptr == ( mtc_cons *) NULL )
  {
  printsend("error: malloc in mtc_init\n");
  free(mtc);
  free(mtc_cons_ptr);
  return -1;
  }

  for (i=0; i<=13; i++)
  { 
  raw_dacs[i]=
  (u_short) mtc->mtca.triggers[i].threshold;
  mtc_cons_ptr->mtca_dac_values[i]=
  (((float)raw_dacs[i]/2048) * 5000.0) - 5000.0;
  }

  load_mtc_dacs(mtc_cons_ptr);

  // clear out the control register
  mtc_reg_write(MTCControlReg,0x0);

  // set lockout width
  lkwidth = (u_short)((~((u_short) mtc->mtcd.lockout_width) & 0xff)*20);
  result += set_lockout_width(lkwidth);

  // zero out gt counter
  set_gt_counter(0);

  // load prescaler
  pscale = ~((u_short)(mtc->mtcd.nhit100_lo_prescale)) + 1;
  result += set_prescale(pscale);

  // load pulser
  freq = 781250.0/(float)((u_long)(mtc->mtcd.pulser_period) + 1);
  result += set_pulser_frequency(freq);

  // setup pedestal width
  pwid = (u_short)(((~(u_short)(mtc->mtcd.pedestal_width)) & 0xff) * 5);
  result += set_pedestal_width(pwid);

  // setup PULSE_GT delays
  printsend("Setting up PULSE_GT delays...\n");
  coarse_delay = (u_short)(((~(u_short)(mtc->mtcd.coarse_delay))
  & 0xff) * 10);
  result += set_coarse_delay(coarse_delay);
  fine_delay = (float)(mtc->mtcd.fine_delay)*
  (float)(mtc->mtcd.fine_slope);
  fdelay_set = set_fine_delay(fine_delay);
  printsend( "PULSE_GET total delay has been set to %f\n",
  (float) coarse_delay+fine_delay+
  (float)(mtc->mtcd.min_delay_offset));

  // load 10 MHz counter???? guess not

  // reset memory
  reset_memory();

  free(mtc);
  free(mtc_cons_ptr);

  if (result < 0) {
  printsend("errors in the MTC initialization!\n");
  return -1;
  }

  printsend("MTC finished initializing\n");
  pr_free(response);
   */
  free(mtc);

  sbc_lock = 0;
  thread_done[thread_num] = 1;
}
