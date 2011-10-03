/** \file xl3_cmds.h */

#ifndef __XL3_CMDS_H
#define __XL3_CMDS_H

int xrw(char *buffer, int rw);
void *pt_xrw(void *args);
int debugging_mode(char *buffer, int onoff);
void *pt_debugging_mode(void *args);
int cmd_change_mode(char *buffer);
void *pt_cmd_change_mode(void* args);
int sm_reset(char *buffer);
void *pt_sm_reset(void* args);
int cmd_xl3_rw(char *buffer);
void *pt_cmd_xl3_rw(void* args);
int xl3_queue_rw(char *buffer);
void *pt_xl3_queue_rw(void* args);
int read_local_voltage(char *buffer);
void *pt_read_local_voltage(void* args);
int hv_readback(char *buffer);
void *pt_hv_readback(void* args);
int set_alarm_dac(char *buffer);
void *pt_set_alarm_dac(void* args);


typedef struct{
  int thread_num;
  int rw;
  int crate_num;
  int register_num;
  uint32_t data;
} xrw_t;

typedef struct{
  int thread_num;
  int crate_num;
} sm_reset_t;

typedef struct{
  int thread_num;
  int crate_num;
  uint32_t address;
  uint32_t data;
} cmd_xl3_rw_t;

typedef struct{
  int thread_num;
  int crate_num;
  int mode;
  uint16_t davail_mask; 
} cmd_change_mode_t;

typedef struct{
  int thread_num;
  int crate_num;
  int onoff;
} debugging_mode_t;

typedef struct{
  int thread_num;
  int crate_num;
  int voltage_select;
} read_local_voltage_t;

typedef struct{
  int thread_num;
  int crate_num;
  int show_a;
  int show_b;
} hv_readback_t;

typedef struct{
  int thread_num;
  int crate_num;
  uint32_t dac0;
  uint32_t dac1;
  uint32_t dac2;
} set_alarm_dac_t;

#endif
