/** \file mtc_utils.h */

#ifndef __MTC_UTILS_H
#define __MTC_UTILS_H

#define MAX_DATA_SIZE                   150000 // max number of allowed bits
#define MTC_CLOCK                       0x00000002UL //sets CCLK high
#define MTC_SET_DP                      0x00000004UL //sets DP high
#define MTC_DRIVE_DP                    0x00000008UL //enables DP buffer
#define MTC_DONE_PROG                   0x00000010UL //DP readback

typedef struct{
  int thread_num;
  int sbc_action;
  int manual;
  char identity_file[100];
} sbc_control_t;

int sbc_control(char *buffer);
void *pt_sbc_control(void *args);

int mtc_xilinx_load();
static char* getXilinxData(long *howManyBits);

#endif
