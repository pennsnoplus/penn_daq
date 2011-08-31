#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "xl3_registers.h"

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "fec_utils.h"

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
