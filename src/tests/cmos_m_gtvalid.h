/** \file cmos_m_gtvalid.h */

#ifndef __FEC_TEST_H
#define __FEC_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t pattern;
  float gt_cutoff;
  int do_twiddle;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} cmos_m_gtvalid_t;

int cmos_m_gtvalid(char *buffer);
void *pt_cmos_m_gtvalid(void *args);

int get_gtdelay(int crate_num, int wt, float *get_gtchan, uint16_t isetm0, uint16_t isetm1, int slot, fd_set *thread_fdset);

#endif
