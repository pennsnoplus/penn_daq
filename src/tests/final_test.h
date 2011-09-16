/** \file final_test.h */

#ifndef __FINAL_TEST_H
#define __FINAL_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int tub_tests;
  int skip;
} final_test_t;

int final_test(char *buffer);
void *pt_final_test(void *args);

#endif
