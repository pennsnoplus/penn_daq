/** \file trigger_scan.h */

#ifndef __TRIGGER_SCAN_H
#define __TRIGGER_SCAN_H

#include <stdint.h>

typedef struct{
  int thread_num;
  int trigger;
  uint32_t crate_mask;
  int min_thresh;
  int thresh_dac;
  int quick_mode;
  int total_nhit;
  uint16_t slot_mask[16];
  char filename[100];
} trigger_scan_t;

int trigger_scan(char *buffer);
void *pt_trigger_scan(void *args);

#endif
