#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "packet_types.h"
#include "mtc_registers.h"
#include "pouch.h"
#include "json.h"

#include "main.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_rw.h"
#include "mtc_utils.h"

int get_gt_count(uint32_t *count)
{
  mtc_reg_read(MTCOcGtReg, count);
  *count &= 0x00FFFFFFF;
  return 0;
}

float set_gt_delay(float gtdel)
{
  int result;
  float offset_res, fine_delay, total_delay, fdelay_set;
  uint16_t cdticks, coarse_delay;

  offset_res = gtdel - (float)(18.35); //FIXME there is delay_offset in db?? check old code
  cdticks = (uint16_t) (offset_res/10.0);
  coarse_delay = cdticks*10;
  fine_delay = offset_res - ((float) cdticks*10.0);
  result = set_coarse_delay(coarse_delay);
  fdelay_set = set_fine_delay(fine_delay);
  total_delay = ((float) coarse_delay + fdelay_set + (float)(18.35));
  return total_delay;
}

int send_softgt()
{
  mtc_reg_write(MTCSoftGTReg,0x0);
  return 0;
}

int multi_softgt(int number)
{
  SBC_Packet *packet;
  packet = malloc(sizeof(SBC_Packet));
  packet->cmdHeader.destination = 0x3;
  packet->cmdHeader.cmdID = 0x5;
  packet->cmdHeader.numberBytesinPayload  = sizeof(SBC_VmeWriteBlockStruct) + sizeof(uint32_t);
  //packet->numBytes = 256+28+16;
  SBC_VmeWriteBlockStruct *writestruct;
  writestruct = (SBC_VmeWriteBlockStruct *) packet->payload;
  writestruct->address = MTCSoftGTReg + MTCRegAddressBase;
  writestruct->addressModifier = MTCRegAddressMod;
  writestruct->addressSpace = MTCRegAddressSpace;
  writestruct->unitSize = 4;
  writestruct->numItems = number;
  writestruct++;
  uint32_t *data_ptr = (uint32_t *) writestruct;
  *data_ptr = 0x0;
  do_mtc_cmd(packet);
  free(packet);


}

int setup_pedestals(float pulser_freq, uint32_t ped_width, 	uint32_t coarse_delay,
    uint32_t fine_delay, uint32_t ped_crate_mask, uint32_t gt_crate_mask)
{
  int result = 0;
  float fdelay_set;
  result += set_lockout_width(DEFAULT_LOCKOUT_WIDTH);
  if (result == 0)
    result += set_pulser_frequency(pulser_freq);
  if (result == 0)
    result += set_pedestal_width(ped_width);
  if (result == 0)
    result += set_coarse_delay(coarse_delay);
  if (result == 0)
    fdelay_set = set_fine_delay(fine_delay);
  if (result != 0){
    pt_printsend("setup pedestals failed\n");
    return -1;
  }
  enable_pulser();
  enable_pedestal();
  unset_ped_crate_mask(MASKALL);
  unset_gt_crate_mask(MASKALL);
  set_ped_crate_mask(ped_crate_mask);
  set_gt_crate_mask(gt_crate_mask);
  set_gt_mask(DEFAULT_GT_MASK);
  //printsend("new_daq: setup_pedestals complete\n");
  return 0;
}

void enable_pulser()
{
    uint32_t temp;
    mtc_reg_read(MTCControlReg,&temp);
    mtc_reg_write(MTCControlReg,temp | PULSE_EN);
    //pt_printsend("Pulser enabled\n");
}

void disable_pulser()
{
    uint32_t temp;
    mtc_reg_read(MTCControlReg,&temp);
    mtc_reg_write(MTCControlReg,temp & ~PULSE_EN);
    //pt_printsend("Pulser disabled\n");
}

void enable_pedestal()
{
    uint32_t temp;
    mtc_reg_read(MTCControlReg,&temp);
    mtc_reg_write(MTCControlReg,temp | PED_EN);
    //pt_printsend("Pedestals enabled\n");
}

void disable_pedestal()
{
    uint32_t temp;
    mtc_reg_read(MTCControlReg,&temp);
    mtc_reg_write(MTCControlReg,temp & ~PED_EN);
    //pt_printsend("Pedestals disabled\n");
}

int load_mtca_dacs_by_counts(uint16_t *raw_dacs)
{
  char dac_names[][14]={"N100LO","N100MED","N100HI","NHIT20","NH20LB","ESUMHI",
    "ESUMLO","OWLEHI","OWLELO","OWLN","SPARE1","SPARE2",
    "SPARE3","SPARE4"};
  int i, j, bi, di;
  float mV_dacs;
  uint32_t shift_value;

  for (i=0;i<14;i++){
    float mV_dacs = (((float)raw_dacs[i]/2048) * 5000.0) - 5000.0;
    pt_printsend( "\t%s\t threshold set to %6.2f mVolts (%d counts)\n", dac_names[i],
        mV_dacs,raw_dacs[i]);
  }

   /* set DACSEL */
  mtc_reg_write(MTCDacCntReg,DACSEL);

  /* shift in raw DAC values */

  for (i = 0; i < 4 ; i++) {
    mtc_reg_write(MTCDacCntReg,DACSEL | DACCLK); /* shift in 0 to first 4 dummy bits */
    mtc_reg_write(MTCDacCntReg,DACSEL);
  }

  shift_value = 0UL;
  for (bi = 11; bi >= 0; bi--) {                     /* shift in 12 bit word for each DAC */
    for (di = 0; di < 14 ; di++){
      if (raw_dacs[di] & (1UL << bi))
        shift_value |= (1UL << di);
      else
        shift_value &= ~(1UL << di);
    }
    mtc_reg_write(MTCDacCntReg,shift_value | DACSEL);
    mtc_reg_write(MTCDacCntReg,shift_value | DACSEL | DACCLK);
    mtc_reg_write(MTCDacCntReg,shift_value | DACSEL);
  }
  /* unset DASEL */
  mtc_reg_write(MTCDacCntReg,0x0);
  return 0;
}

int load_mtca_dacs(float *voltages)
{
  uint16_t raw_dacs[14];
  int i;
    pt_printsend("Loading MTC/A threshold DACs...\n");


  /* convert each threshold from mVolts to raw value and load into
     raw_dacs array */
  for (i = 0; i < 14; i++) {
    //raw_dacs[i] = ((2048 * rdbuf)/5000) + 2048;
    raw_dacs[i] = MTCA_DAC_SLOPE * voltages[i] + MTCA_DAC_OFFSET;
  }

  load_mtca_dacs_by_counts(raw_dacs);
 
  pt_printsend("DAC loading complete\n");
  return 0;
}



void unset_gt_mask(uint32_t raw_trig_types)
{
  uint32_t temp;
  mtc_reg_read(MTCMaskReg, &temp);
  mtc_reg_write(MTCMaskReg, temp & ~raw_trig_types);
  //pt_printsend("Triggers have been removed from the GT Mask\n");
}

void set_gt_mask(uint32_t raw_trig_types)
{
  uint32_t temp;
  mtc_reg_read(MTCMaskReg, &temp);
  mtc_reg_write(MTCMaskReg, temp | raw_trig_types);
  //pt_printsend("Triggers have been added to the GT Mask\n");
}

void unset_ped_crate_mask(uint32_t crates)
{
  uint32_t temp;
  mtc_reg_read(MTCPmskReg, &temp);
  mtc_reg_write(MTCPmskReg, temp & ~crates);
  //printsend("Crates have been removed from the Pedestal Crate Mask\n");
}

void set_ped_crate_mask(uint32_t crates)
{
  if (CURRENT_LOCATION == PENN_TESTSTAND)
    crates = MASKALL;
  uint32_t temp;
  mtc_reg_read(MTCPmskReg, &temp);
  mtc_reg_write(MTCPmskReg, temp | crates);
  //printsend("Crates have been added to the Pedestal Crate Mask\n");
}

void unset_gt_crate_mask(uint32_t crates)
{
  uint32_t temp;
  mtc_reg_read(MTCGmskReg, &temp);
  mtc_reg_write(MTCGmskReg, temp & ~crates);
  //printsend("Crates have been removed from the GT Crate Mask\n");
}

void set_gt_crate_mask(uint32_t crates)
{
  if (CURRENT_LOCATION == PENN_TESTSTAND)
    crates = MASKALL;
  uint32_t temp;
  mtc_reg_read(MTCGmskReg, &temp);
  mtc_reg_write(MTCGmskReg, temp | crates);
  //printsend("Crates have been added to the GT Crate Mask\n");
}

int set_lockout_width(uint16_t width)
{
  uint32_t gtlock_value;
  if ((width < 20) || (width > 5100)) {
    pt_printsend("Lockout width out of range\n");
    return -1;
  }
  gtlock_value = ~(width / 20);
  uint32_t temp;
  mtc_reg_write(MTCGtLockReg,gtlock_value);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp | LOAD_ENLK); /* write GTLOCK value */
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp & ~LOAD_ENLK); /* toggle load enable */
  //pt_printsend( "Lockout width is set to %u ns\n", width);
  return 0;
}

int set_gt_counter(uint32_t count)
{
  uint32_t shift_value;
  short j;
  uint32_t temp;

  for (j = 23; j >= 0; j--){
    shift_value = ((count >> j) & 0x01) == 1 ? SERDAT | SEN : SEN ;
    mtc_reg_write(MTCSerialReg,shift_value);
    mtc_reg_read(MTCSerialReg,&temp);
    mtc_reg_write(MTCSerialReg,temp | SHFTCLKGT); /* clock in SERDAT */
  }
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp | LOAD_ENGT); /* toggle load enable */
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp & ~LOAD_ENGT); /* toggle load enable */

  pt_printsend("The GT counter has been loaded. It is now set to %d\n",(int) count);
  return 0;
}

int set_prescale(uint16_t scale)
{
  uint32_t temp;
  if (scale < 2) {
    pt_printsend("Prescale value out of range\n");
    return -1;
  }
  mtc_reg_write(MTCScaleReg,~(scale-1));
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp | LOAD_ENPR);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp & ~LOAD_ENPR); /* toggle load enable */
  pt_printsend( "Prescaler set to %d NHIT_100_LO per PRESCALE\n", scale);
  return 0;
}     

int set_pulser_frequency(float freq)
{
  uint32_t pulser_value, shift_value, prog_freq;
  int16_t j;
  uint32_t temp;

  if (freq <= 1.0e-3) {                                /* SOFT_GTs as pulser */
    pulser_value = 0;
    pt_printsend("SOFT_GT is set to source the pulser\n");
  }
  else {
    pulser_value = (uint32_t)((781250 / freq) - 1);   /* 50MHz counter as pulser */
    prog_freq = (uint32_t)(781250/(pulser_value + 1));
    if ((pulser_value < 1) || (pulser_value > 167772216)) {
      pt_printsend( "Pulser frequency out of range\n", prog_freq);
      return -1;
    }
  }

  for (j = 23; j >= 0; j--){
    shift_value = ((pulser_value >> j) & 0x01) == 1 ? SERDAT|SEN : SEN; 
    mtc_reg_write(MTCSerialReg,shift_value);
    mtc_reg_read(MTCSerialReg,&temp);
    mtc_reg_write(MTCSerialReg,temp | SHFTCLKPS); /* clock in SERDAT */
  }
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp | LOAD_ENPS);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp & ~LOAD_ENPS); /* toggle load enable */
  //pt_printsend( "Pulser frequency is set to %u Hz\n", prog_freq);
  return 0;
}

int set_pedestal_width(uint16_t width)
{
  uint32_t temp, pwid_value;
  if ((width < 5) || (width > 1275)) {
    pt_printsend("Pedestal width out of range\n");
    return -1;
  }
  pwid_value = ~(width / 5);

  mtc_reg_write(MTCPedWidthReg,pwid_value);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg, temp | LOAD_ENPW);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg, temp & ~LOAD_ENPW);
  //pt_printsend( "Pedestal width is set to %u ns\n", width);
  return 0;
}

int set_coarse_delay(uint16_t delay)
{
  uint32_t temp, rtdel_value;

  if ((delay < 10) || (delay > 2550)) {
    pt_printsend("Coarse delay value out of range\n");
    return -1;
  } 
  rtdel_value = ~(delay / 10);

  mtc_reg_write(MTCCoarseDelayReg,rtdel_value);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg, temp | LOAD_ENPW);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg, temp & ~LOAD_ENPW);
  //pt_printsend( "Coarse delay is set to %u ns\n", delay);
  return 0;
} 

float set_fine_delay(float delay)
{
  uint32_t temp, addel_value;;
  int result;
  float addel_slope;   /* ADDEL value per ns of delay */
  float fdelay_set;

  //addel_slope = 0.1;
  // get up to date fine slope value
  pouch_request *response = pr_init();
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/MTC_doc",DB_SERVER,DB_BASE_NAME);
  pr_set_method(response, GET);
  pr_set_url(response, get_db_address);
  pr_do(response);
  if (response->httpresponse != 200){
    pt_printsend("Unable to connect to database. error code %d\n",(int)response->httpresponse);
    pr_free(response);
    return -1.0;
  }
  JsonNode *doc = json_decode(response->resp.data);
  addel_slope = (float) json_get_number(json_find_member(json_find_member(doc,"mtcd"),"fine_slope")); 
  addel_value = (uint32_t)(delay / addel_slope);
  json_delete(doc);
  pr_free(response);

  if (addel_value > 255) {
    pt_printsend("Fine delay value out of range\n");
    return -1.0;
  }
  addel_value = (uint32_t)(delay / addel_slope);

  mtc_reg_write(MTCFineDelayReg,addel_value);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg, temp | LOAD_ENPW);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg, temp & ~LOAD_ENPW);

  fdelay_set = (float)addel_value*addel_slope;
  //pt_printsend( "Fine delay is set to %f ns\n", fdelay_set);
  return fdelay_set;
}

void reset_memory()
{
  uint32_t temp;
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp | FIFO_RESET);
  mtc_reg_read(MTCControlReg,&temp);
  mtc_reg_write(MTCControlReg,temp & ~FIFO_RESET);
  mtc_reg_write(MTCBbaReg,0x0);  
  pt_printsend("The FIFO control has been reset and the BBA register has been cleared\n");
} 

int mtc_xilinx_load()
{
  char *data;
  long howManybits;
  long bitIter;
  uint32_t word;
  uint32_t dp;
  uint32_t temp;

  pt_printsend("loading xilinx\n");
  data = getXilinxData(&howManybits);
  pt_printsend("Got %d bits of xilinx data\n",howManybits);
  if ((data == NULL) || (howManybits == 0)){
    pt_printsend("error getting xilinx data\n");
    return -1;
  }

  SBC_Packet *packet; 
  packet = malloc(sizeof(SBC_Packet));

  packet->cmdHeader.destination = 0x3;
  packet->cmdHeader.cmdID = MTC_XILINX_ID;
  packet->cmdHeader.numberBytesinPayload = sizeof(SNOMtc_XilinxLoadStruct) + howManybits;
  packet->numBytes = packet->cmdHeader.numberBytesinPayload+256+16;
  SNOMtc_XilinxLoadStruct *payloadPtr = (SNOMtc_XilinxLoadStruct *)packet->payload;
  payloadPtr->baseAddress = 0x7000;
  payloadPtr->addressModifier = 0x29;
  payloadPtr->errorCode = 0;
  payloadPtr->programRegOffset = MTCXilProgReg;
  payloadPtr->fileSize = howManybits;
  char *p = (char *)payloadPtr + sizeof(SNOMtc_XilinxLoadStruct);
  strncpy(p, data, howManybits);
  free(data);
  data = (char *) NULL;

  int errors = do_mtc_xilinx_cmd(packet);
  long errorCode = payloadPtr->errorCode;
  if (errorCode){
    pt_printsend("Error code: %d \n",(int)errorCode);
    pt_printsend("Failed to load xilinx!\n");  
    free(packet);
    return -5;
  }else if (errors < 0){
    pt_printsend("Failed to load xilinx!\n");  
    free(packet);
    return errors;
  }

  free(packet);
  pt_printsend("Xilinx loading complete\n");
  return 0;
}

static char* getXilinxData(long *howManyBits)
{
  char c;
  FILE *fp;
  char *data = NULL;

  if ((fp = fopen(MTC_XILINX_LOCATION, "r")) == NULL ) {
    pt_printsend( "getXilinxData:  cannot open file %s\n", MTC_XILINX_LOCATION);
    return (char*) NULL;
  }

  if ((data = (char *) malloc(MAX_DATA_SIZE)) == NULL) {
    //perror("GetXilinxData: ");
    pt_printsend("GetXilinxData: malloc error\n");
    return (char*) NULL;
  }

  /* skip header -- delimited by two slashes. 
     if ( (c = getc(fp)) != '/') {
     fprintf(stderr, "Invalid file format Xilinx file.\n");
     return (char*) NULL;
     }
     while (( (c = getc(fp))  != EOF ) && ( c != '/'))
     ;
   */
  /* get real data now. */
  *howManyBits = 0;
  while (( (data[*howManyBits] = getc(fp)) != EOF)
      && ( *howManyBits < MAX_DATA_SIZE)) {
    /* skip newlines, tabs, carriage returns */
    if ((data[*howManyBits] != '\n') &&
        (data[*howManyBits] != '\r') &&
        (data[*howManyBits] != '\t') ) {
      (*howManyBits)++;
    }


  }
  fclose(fp);
  return data;
}

