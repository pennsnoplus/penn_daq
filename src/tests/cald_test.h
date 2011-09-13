/** \file cald_test.h */

#ifndef __CALD_TEST_H
#define __CALD_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int upper;
  int lower;
  int num_points;
  int samples;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} cald_test_t;

int cald_test(char *buffer);
void *pt_cald_test(void *args);


#endif
