/** \file ecal.h */

#ifndef __ECAL_H
#define __ECAL_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_mask;
  uint16_t slot_mask[16];
  int tub_tests;
  int skip;
} ecal_t;

int ecal(char *buffer);
void *pt_ecal(void *args);

#endif
