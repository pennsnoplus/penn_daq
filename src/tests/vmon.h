/** \file vmon.h */

#ifndef __VMON_H
#define __VMON_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint32_t slot_mask;;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} vmon_t;

int vmon(char *buffer);
void *pt_vmon(void *args);

#endif
