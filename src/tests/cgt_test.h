/** \file cgt_test.h */

#ifndef __CGT_TEST_H
#define __CGT_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t pattern;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} cgt_test_t;

int cgt_test(char *buffer);
void *pt_cgt_test(void *args);

#endif
