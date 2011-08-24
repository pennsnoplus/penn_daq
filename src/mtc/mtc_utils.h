/** \file mtc_utils.h */

#ifndef __MTC_UTILS_H
#define __MTC_UTILS_H


typedef struct{
  int thread_num;
  int sbc_action;
  char identity_file[100];
} sbc_control_t;

int sbc_control(char *buffer);
void *pt_sbc_control(void *args);

#endif
