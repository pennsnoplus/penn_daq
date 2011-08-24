/** \file crate_init.h */

#ifndef __CRATE_INIT_H
#define __CRATE_INIT_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  int xilinx_load;
  int hv_reset;
  int shift_reg_only;
  uint16_t slot_mask;
  int use_cbal;
  int use_zdisc;
  int use_ttot;
  int use_all;
  int use_hw;
} crate_init_t;

int crate_init(char *buffer);
void *pt_crate_init(void *args);

#endif
