/** \file crate_cbal.h */

#ifndef __CRATE_CBAL_H
#define __CRATE_CBAL_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} crate_cbal_t;

int crate_cbal(char *buffer);
void *pt_crate_cbal(void *args);

#endif
