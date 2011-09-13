/** \file ttot.h */

#ifndef __TTOT_H
#define __TTOT_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int target_time;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} get_ttot_t;

int get_ttot(char *buffer);
void *pt_get_ttot(void *args);

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int target_time;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} set_ttot_t;

int set_ttot(char *buffer);
void *pt_set_ttot(void *args);


int disc_m_ttot(int crate, uint32_t slot_mask, int start_time, uint16_t *disc_times, fd_set *thread_fdset);
  
#endif
