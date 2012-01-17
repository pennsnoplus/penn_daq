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

int deselect_fecs(int crate, fd_set *thread_fdset)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = DESELECT_FECS_ID;
  do_xl3_cmd(&packet, crate,thread_fdset);
  return 0;
}

int update_crate_config(int crate, uint16_t slot_mask, fd_set *thread_fdset)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = BUILD_CRATE_CONFIG_ID;
  build_crate_config_args_t *packet_args = (build_crate_config_args_t *) packet.payload;
  build_crate_config_results_t *packet_results = (build_crate_config_results_t *) packet.payload;
  packet_args->slot_mask = slot_mask;
  SwapLongBlock(packet.payload,sizeof(build_crate_config_args_t)/sizeof(uint32_t));
  printf("lock is %d\n",xl3_lock[crate]);
  if (FD_ISSET(rw_xl3_fd[crate],thread_fdset))
    printf("is set\n");
  else
    printf("is not set\n");
  do_xl3_cmd(&packet,crate,thread_fdset);
  printf("done\n");
  int errors = packet_results->error_flags;
  int i;
  for (i=0;i<16;i++){
    if ((0x1<<i) & slot_mask){
      crate_config[crate][i] = packet_results->hware_vals[i];
      SwapShortBlock(&(crate_config[crate][i].mb_id),1);
      SwapShortBlock((crate_config[crate][i].db_id),4);
    }
  }
  deselect_fecs(crate,thread_fdset);
  return 0;
}

int change_mode(int mode, uint16_t davail_mask, int xl3num, fd_set *thread_fdset)
{
  XL3_Packet packet;
  packet.cmdHeader.packet_type = CHANGE_MODE_ID;
  change_mode_args_t *packet_args = (change_mode_args_t *) packet.payload;
  packet_args->mode = mode;
  packet_args->davail_mask = davail_mask;
  SwapLongBlock(packet.payload,sizeof(change_mode_args_t)/sizeof(uint32_t));
  int result = do_xl3_cmd(&packet,xl3num,thread_fdset);
  return result;
}
