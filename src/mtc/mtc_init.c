#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pouch.h"
#include "json.h"
#include "mtc_registers.h"

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

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0,&new_thread);
  if (thread_num < 0){
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

  pt_printsend("Initializing MTC/A/D\n");

  int errors;

  if (xilinx_load)
    errors = mtc_xilinx_load();

  if (errors < 0){
    unthread_and_unlock(1,0x0,thread_num);
    return;
  }

  mtc_t *mtc = ( mtc_t * ) malloc( sizeof(mtc_t));
  if ( mtc == ( mtc_t *) NULL )
  {
    pt_printsend("error: malloc in mtc_init\n");
    free(mtc);
    unthread_and_unlock(1,0x0,thread_num);
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
    free(response);
    free(mtc);
    unthread_and_unlock(1,0x0,thread_num);
    return;
  }
  JsonNode *doc = json_decode(response->resp.data);
  parse_mtc(doc,mtc);
  json_delete(doc);
  pr_free(response);

  // hold errors here
  int result = 0;
 
  //unset all masks
  unset_gt_mask(MASKALL);
  unset_ped_crate_mask(MASKALL);
  unset_gt_crate_mask(MASKALL);

  // load the dacs
  float mtca_dac_values[14];

  int i;
  for (i=0; i<=13; i++)
  { 
    uint16_t raw_dacs = (uint16_t) mtc->mtca.triggers[i].threshold;
    mtca_dac_values[i] = (((float) raw_dacs/2048) * 5000.0) - 5000.0;
  }

  load_mtca_dacs(mtca_dac_values);

  // clear out the control register
  mtc_reg_write(MTCControlReg,0x0);

  // set lockout width
  uint16_t lkwidth = (uint16_t)((~((u_short) mtc->mtcd.lockout_width) & 0xff)*20);
  result += set_lockout_width(lkwidth);

  // zero out gt counter
  set_gt_counter(0);

  // load prescaler
  uint16_t pscale = ~((uint16_t)(mtc->mtcd.nhit100_lo_prescale)) + 1;
  result += set_prescale(pscale);

  // load pulser
  float freq = 781250.0/(float)((u_long)(mtc->mtcd.pulser_period) + 1);
  result += set_pulser_frequency(freq);

  // setup pedestal width
  uint16_t pwid = (uint16_t)(((~(u_short)(mtc->mtcd.pedestal_width)) & 0xff) * 5);
  result += set_pedestal_width(pwid);


  // setup PULSE_GT delays
  pt_printsend("Setting up PULSE_GT delays...\n");
  uint16_t coarse_delay = (uint16_t)(((~(uint16_t)(mtc->mtcd.coarse_delay)) & 0xff) * 10);
  result += set_coarse_delay(coarse_delay);
  float fine_delay = (float)(mtc->mtcd.fine_delay)*(float)(mtc->mtcd.fine_slope);
  float fdelay_set = set_fine_delay(fine_delay);
  pt_printsend( "PULSE_GET total delay has been set to %f\n",
      (float) coarse_delay+fine_delay+
      (float)(mtc->mtcd.min_delay_offset));

  // load 10 MHz counter???? guess not

  // reset memory
  reset_memory();

  free(mtc);

  if (result < 0) {
    pt_printsend("errors in the MTC initialization!\n");
  }else{
    pt_printsend("MTC finished initializing\n");
  }

  unthread_and_unlock(1,0x0,thread_num);
}
