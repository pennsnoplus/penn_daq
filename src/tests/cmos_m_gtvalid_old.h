/** \file cmos_m_gtvalid_old.h */

#ifndef __CMOS_M_GTVALID_OLD_H
#define __CMOS_M_GTVALID_OLD_H

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
  int ecal;
  char ecal_id[50];
} cmos_m_gtvalid_old_t;

int cmos_m_gtvalid_old(char *buffer);
void *pt_cmos_m_gtvalid_old(void *args);

int get_gtdelay_old(int crate_num, int wt, float *get_gttac0, float *get_gttac1, uint16_t isetm_orig, int slot, fd_set *thread_fdset);

#endif
