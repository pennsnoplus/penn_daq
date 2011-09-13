/** \file chinj_scan.h */

#ifndef __CHINJ_SCAN_H
#define __CHINJ_SCAN_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t pattern;
  float frequency;
  uint32_t gtdelay;
  uint16_t ped_width;
  int num_pedestals;
  float chinj_lower;
  float chinj_upper;
  int q_select;
  int ped_on;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} chinj_scan_t;

int chinj_scan(char *buffer);
void *pt_chinj_scan(void *args);

int setup_chinj(int crate, uint16_t slot_mask, uint32_t default_ch_mask, uint16_t dacvalue, fd_set *thread_fdset);

#endif
