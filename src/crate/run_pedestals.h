/** \file run_pedestals.h */

#ifndef __RUN_PEDESTALS_H
#define __RUN_PEDESTALS_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int mtc;
  int crate;
  uint32_t crate_mask;
  uint16_t slot_mask[19];
  uint32_t pattern;
  float frequency;
  uint32_t gtdelay;
  uint16_t ped_width;
} run_pedestals_t;

int run_pedestals(char *buffer,int mtc, int crate);
void *pt_run_pedestals(void *args);
int run_pedestals_end(char *buffer, int mtc, int crate);
void *pt_run_pedestals_end(void *args);

#endif
