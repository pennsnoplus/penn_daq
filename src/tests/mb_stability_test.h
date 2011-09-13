/** \file mb_stability_test.h */

#ifndef __MB_STABILITY_TEST_H
#define __MB_STABILITY_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t num_pedestals;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} mb_stability_test_t;

int mb_stability_test(char *buffer);
void *pt_mb_stability_test(void *args);

#endif
