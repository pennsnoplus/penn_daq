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
} ped_run_t;

int ped_run(char *buffer);
void *pt_ped_run(void *args);

struct cell {
  int16_t per_cell;
  int cellno;
  double qlxbar, qlxrms;
  double qhlbar, qhlrms;
  double qhsbar, qhsrms;
  double tacbar, tacrms;
};

struct pedestal {
  int16_t channelnumber;
  int16_t per_channel;
  struct cell thiscell[16];
};

#endif
