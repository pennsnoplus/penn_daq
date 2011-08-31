/** \file mtc_cmds.h */

#ifndef __MTC_CMDS_H
#define __MTC_CMDS_H

int sbc_control(char *buffer);
void *pt_sbc_control(void *args);
int mtc_read(char *buffer);
void *pt_mtc_read(void *args);
int mtc_write(char *buffer);
void *pt_mtc_write(void *args);
int set_mtca_thresholds(char *buffer);
void *pt_set_mtca_thresholds(void *args);
int cmd_set_gt_mask(char *buffer);
void *pt_cmd_set_gt_mask(void *args);
int cmd_set_gt_crate_mask(char *buffer);
void *pt_cmd_set_gt_crate_mask(void *args);
int cmd_set_ped_crate_mask(char *buffer);
void *pt_cmd_set_ped_crate_mask(void *args);

typedef struct{
  int thread_num;
  int sbc_action;
  int manual;
  char identity_file[100];
} sbc_control_t;

typedef struct{
  int thread_num;
  uint32_t address;
} mtc_read_t;

typedef struct{
  int thread_num;
  uint32_t address;
  uint32_t data;
} mtc_write_t;

typedef struct{
  int thread_num;
  float dac_voltages[14];
} set_mtca_thresholds_t;

typedef struct{
  int thread_num;
  uint32_t mask;
  int ored;
} set_mask_t;

#endif
