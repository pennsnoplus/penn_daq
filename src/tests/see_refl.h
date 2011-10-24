/** \file see_refl.h */

#ifndef __SEE_REFL_H
#define __SEE_REFL_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int dac_value;
  int crate_num;
  uint16_t slot_mask;
  uint32_t pattern;
  float frequency;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} see_refl_t;

int see_refl(char *buffer);
void *pt_see_refl(void *args);

#endif
