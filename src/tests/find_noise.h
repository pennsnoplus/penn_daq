/** \file find_noise.h */

#ifndef __FIND_NOISE_H
#define __FIND_NOISE_H

#include <stdint.h>

typedef struct{
  int thread_num;
  uint32_t crate_mask;
  uint16_t slot_mask[16];
  int update_db;
  int use_debug;
  int ecal;
  char ecal_id[50];
} find_noise_t;

int find_noise(char *buffer);
void *pt_find_noise(void *args);

#endif
