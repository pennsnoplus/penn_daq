#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"

#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "xl3_utils.h"

int deselect_fecs(int crate)
{
  return 0;
}

int update_crate_config(int crate, uint16_t slot_mask)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = BUILD_CRATE_CONFIG_ID;
  *(uint32_t *) packet.payload = slot_mask;
  SwapLongBlock(packet.payload,1);
  do_xl3_cmd(&packet,crate);

  int errors = *(uint32_t *) packet.payload;
  int j;
  for (j=0;j<16;j++){
    if ((0x1<<j) & slot_mask){
      crate_config[crate][j] = *(hware_vals_t *) (packet.payload+4+j*sizeof(hware_vals_t));
      SwapShortBlock(&(crate_config[crate][j].mb_id),1);
      SwapShortBlock((crate_config[crate][j].dc_id),4);
    }
  }
  deselect_fecs(crate);
  return 0;
}

