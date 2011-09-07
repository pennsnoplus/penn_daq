/** \file mem_test.h */

#ifndef __MEM_TEST_H
#define __MEM_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
} mem_test_t;

int mem_test(char *buffer);
void *pt_mem_test(void *args);

#endif
