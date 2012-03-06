/** \file ecal_to_fec.h */

#ifndef __ECALTOFEC_H
#define __ECALTOFEC_H

#include <stdint.h>

typedef struct{
  int thread_num;
  char ecal_id[50];
  uint32_t testmask;
} ecal_to_fec_t;

int ecal_to_fec(char *buffer);
void *pt_ecal_to_fec(void *args);

#endif
