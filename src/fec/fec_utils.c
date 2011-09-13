#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "xl3_registers.h"
#include "unpack_bundles.h"

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "fec_utils.h"

void dump_pmt_verbose(int n, uint32_t *pmt_buf, char* msg_buf)
{
  int i,j;
  char msg[10000];
  sprintf(msg,"\0");
  for (i=0;i<n*3;i+=3)
  {
    if (!(i%32)){
      sprintf(msg+strlen(msg),"No\tCh\tCell\t\tGT\tQlx\tQhs\tQhl\tTAC\tES\tMC\tLGI\tNC/CC\tCr\tBd\n");
      sprintf(msg+strlen(msg),"--------------------------------------------------------------------------------------\n");
    }
    sprintf(msg+strlen(msg),"% 4d\t%2u\t%2u\t%8u\t%4u\t%4u\t%4u\t%4u\t%u%u%u\t%1u\t%1u\t%1u\t%2u\t%2u\n",
        i/3,
        (uint32_t) UNPK_CHANNEL_ID(pmt_buf+i),
        (uint32_t) UNPK_CELL_ID(pmt_buf+i),
        (uint32_t) UNPK_FEC_GT_ID(pmt_buf+i),
        (uint32_t) UNPK_QLX(pmt_buf+i),
        (uint32_t) UNPK_QHS(pmt_buf+i),
        (uint32_t) UNPK_QHL(pmt_buf+i),
        (uint32_t) UNPK_TAC(pmt_buf+i),
        (uint32_t) UNPK_CMOS_ES_16(pmt_buf+i),
        (uint32_t) UNPK_CGT_ES_16(pmt_buf+i),
        (uint32_t) UNPK_CGT_ES_24(pmt_buf+i),
        (uint32_t) UNPK_MISSED_COUNT(pmt_buf+i),
        (uint32_t) UNPK_LGI_SELECT(pmt_buf+i),
        (uint32_t) UNPK_NC_CC(pmt_buf+i),
        (uint32_t) UNPK_CRATE_ID(pmt_buf+i),
        (uint32_t) UNPK_BOARD_ID(pmt_buf+i));
  }
  sprintf(msg_buf+strlen(msg_buf),"%s",msg);
  pt_printsend("%s",msg);
  return;
}

int get_cmos_total_count(int crate, int slot, uint32_t *total_count, fd_set *thread_fdset)
{
  XL3_Packet packet;
  check_total_count_args_t *packet_args = (check_total_count_args_t *) packet.payload;
  check_total_count_results_t *packet_results = (check_total_count_results_t *) packet.payload;
  packet.cmdHeader.packet_type = CHECK_TOTAL_COUNT_ID;
  packet_args->slot_num = slot;
  SwapLongBlock(packet_args,sizeof(check_total_count_args_t)/sizeof(uint32_t));
  do_xl3_cmd(&packet,crate,thread_fdset);
  SwapLongBlock(packet_results,sizeof(check_total_count_results_t)/sizeof(uint32_t));
  int i;
  for (i=0;i<32;i++)
    total_count[i] = packet_results->count[i];
  return 0;
}

int multi_loadsDac(int num_dacs, uint32_t *dac_nums, uint32_t *dac_values, int crate_num, int slot_num, fd_set *thread_fdset)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = MULTI_LOADSDAC_ID;
  multi_loadsdac_args_t *packet_args = (multi_loadsdac_args_t *) packet.payload;
  multi_loadsdac_results_t *packet_results = (multi_loadsdac_results_t *) packet.payload;
  packet_args->num_dacs = num_dacs;
  int j;
  for (j=0;j<num_dacs;j++){
    packet_args->dacs[j].slot_num = slot_num;
    packet_args->dacs[j].dac_num = dac_nums[j];
    packet_args->dacs[j].dac_value = dac_values[j];
  }
  SwapLongBlock(packet_args,num_dacs*3+1);
  do_xl3_cmd(&packet,crate_num,thread_fdset);
  SwapLongBlock(packet_results,sizeof(multi_loadsdac_results_t)/sizeof(uint32_t));
  return packet_results->error_flags;
}

int loadsDac(uint32_t dac_num, uint32_t dac_value, int crate_num, int slot_num, fd_set *thread_fdset)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = LOADSDAC_ID;
  loadsdac_args_t *packet_args = (loadsdac_args_t *) packet.payload;
  loadsdac_results_t *packet_results = (loadsdac_results_t *) packet.payload;
  packet_args->slot_num = slot_num;
  packet_args->dac_num = dac_num;
  packet_args->dac_value = dac_value;
  SwapLongBlock(packet_args,sizeof(loadsdac_args_t)/sizeof(uint32_t));
  do_xl3_cmd(&packet,crate_num,thread_fdset);
  SwapLongBlock(packet_results,sizeof(loadsdac_results_t)/sizeof(uint32_t));
  return packet_results->error_flags;
}

int32_t read_out_bundles(int crate, int slot, int limit, uint32_t *pmt_buf, fd_set *thread_fdset)
{
  XL3_Packet packet;
  MultiFC *commands = (MultiFC *) packet.payload;
  int count;
  int error = 0;
  uint32_t result,diff;
  uint32_t select_reg = FEC_SEL*slot;

  // find out how many bundles are in the fifo
  xl3_rw(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&diff,crate,thread_fdset);
  diff &= 0xFFFFF;
  diff = diff - (diff%3);

  // check if there are more bundles than expected
  if ((3*limit) < diff){
    if (diff > 1.5*(3*limit))
      pt_printsend("Memory level much higher than expected (%d > %d). "
          "Possible fifo overflow\n",diff,3*limit);
    else
      pt_printsend("Memory level over expected (%d > %d)\n",diff,3*limit);
    diff = 3*limit;
  }else if ((3*limit) > diff){
    pt_printsend("Memory level under expected (%d < %d)\n",diff,3*limit);
  }

  // lets read out the bundles!
  pt_printsend("Attempting to read %d bundles\n",diff/3);
  // we need to read it out MAX_FEC_COMMANDS at a time
  int reads_left = diff;
  int this_read;
  while (reads_left != 0){
    if (reads_left > MAX_FEC_COMMANDS-1000)
      this_read = MAX_FEC_COMMANDS-1000;
    else
      this_read = reads_left;
    // queue up all the reads
    packet.cmdHeader.packet_type = READ_PEDESTALS_ID;
    read_pedestals_args_t *packet_args = (read_pedestals_args_t *) packet.payload;
    packet_args->slot = slot;
    packet_args->reads = this_read;
    SwapLongBlock(packet_args,sizeof(read_pedestals_args_t)/sizeof(uint32_t));
    do_xl3_cmd(&packet,crate,thread_fdset);

    // now wait for the data to come
    wait_for_multifc_results(this_read,command_number[crate]-1,crate,pmt_buf+(diff-reads_left),thread_fdset);
    reads_left -= this_read;
  }

  count = diff / 3;
  deselect_fecs(crate,thread_fdset);
  return count;
}

int fec_load_crate_addr(int crate, uint16_t slot_mask, fd_set *thread_fdset)
{
  int i,errors = 0;
  uint32_t result;
  for (i=0;i<16;i++){
    if ((0x1<<i) & slot_mask){
      uint32_t select_reg = FEC_SEL*i;
      xl3_rw(GENERAL_CSR_R + select_reg + WRITE_REG, 0x0 | (crate << FEC_CSR_CRATE_OFFSET),&result,crate,thread_fdset);
    }
  }
  deselect_fecs(crate,thread_fdset);
  return errors;
}

int set_crate_pedestals(int crate, uint16_t slot_mask, uint32_t pattern, fd_set *thread_fdset)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = SET_CRATE_PEDESTALS_ID;
  set_crate_pedestals_args_t *packet_args = (set_crate_pedestals_args_t *) packet.payload;
  packet_args->slot_mask = slot_mask;
  packet_args->pattern = pattern;
  SwapLongBlock(packet_args,sizeof(set_crate_pedestals_args_t)/sizeof(uint32_t));
  do_xl3_cmd(&packet,crate,thread_fdset);
  return 0;
}
