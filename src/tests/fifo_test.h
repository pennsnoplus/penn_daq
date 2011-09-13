/** \file fifo_test.h */

#ifndef __FIFO_TEST_H
#define __FIFO_TEST_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} fifo_test_t;

int fifo_test(char *buffer);
void *pt_fifo_test(void *args);

static void check_fifo(uint32_t *thediff, int crate_num, int slot_num, char *msg_buff, fd_set *thread_fdset);
  
#endif
