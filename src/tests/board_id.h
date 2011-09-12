/** \file board_id.h */

#ifndef __BOARD_ID_H
#define __BOARD_ID_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
} board_id_t;

int board_id(char *buffer);
void *pt_board_id(void *args);

#endif
