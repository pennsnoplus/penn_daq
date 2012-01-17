/** \file zdisc.h */

#ifndef __ZDISC_H
#define __ZDISC_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  float rate;
  uint32_t offset;
  int update_db;
  int final_test;
  char ft_ids[16][50];
  int ecal;
  char ecal_id[50];
} zdisc_t;

int zdisc(char *buffer);
void *pt_zdisc(void *args);

#endif
