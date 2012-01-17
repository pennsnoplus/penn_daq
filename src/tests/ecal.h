/** \file ecal.h */

#ifndef __ECAL_H
#define __ECAL_H

#include <stdint.h>

typedef struct{
  int thread_num;
  uint32_t crate_mask;
  uint16_t slot_mask[19];
  int update_hwdb;
  int old_ecal;
  char ecal_id[50];
  int noise_run;
} ecal_t;

int ecal(char *buffer);
void *pt_ecal(void *args);

#endif
