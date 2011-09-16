/** \file disc_check.h */

#ifndef __DISC_CHECK_H
#define __DISC_CHECK_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int num_pedestals;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} disc_check_t;

int disc_check(char *buffer);
void *pt_disc_check(void *args);

#endif
