/** \file ped_run.h */

#ifndef __PED_RUN_H
#define __PED_RUN_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t pattern;
  int num_pedestals;
  int ped_low;
  int ped_high;
  float frequency;
  uint32_t gt_delay;
  uint16_t ped_width;
  int balanced;
  int update_db;
  int final_test;
  char ft_ids[16][50];
  int ecal;
  char ecal_id[50];
} ped_run_t;

int ped_run(char *buffer);
void *pt_ped_run(void *args);

#endif
