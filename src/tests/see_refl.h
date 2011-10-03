/** \file see_refl.h */

#ifndef __SEE_REFL_H
#define __SEE_REFL_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int dac_value;
  int crate_num;
  uint32_t pattern;
  int debug;
  float frequency;
  int start_slot;
} see_refl_t;

int see_refl(char *buffer);
void *pt_see_refl(void *args);

#endif
