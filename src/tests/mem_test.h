/** \file mem_test.h */

#ifndef __MEM_TEST_H
#define __MEM_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  int slot_num;
  int update_db;
  int final_test;
  char ft_id[50];
} mem_test_t;

int mem_test(char *buffer);
void *pt_mem_test(void *args);

#endif
