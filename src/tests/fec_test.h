/** \file fec_test.h */

#ifndef __FEC_TEST_H
#define __FEC_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint32_t slot_mask;
  int update_db;
  int final_test;
  char ft_ids[16][50];
  int ecal;
  char ecal_id[50];
} fec_test_t;

int fec_test(char *buffer);
void *pt_fec_test(void *args);

#endif
