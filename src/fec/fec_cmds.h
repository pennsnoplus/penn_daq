/** \file fec_cmds.h */

#ifndef __FEC_CMDS_H
#define __FEC_CMDS_H

int frw(char *buffer, int rw);
void *pt_frw(void *args);
int read_bundle(char *buffer);
void *pt_read_bundle(void* args);
int load_relays(char *buffer);
void *pt_load_relays(void* args);
int cmd_setup_chinj(char *buffer);
void *pt_cmd_setup_chinj(void* args);
int cmd_load_dac(char *buffer);
void *pt_cmd_load_dac(void *args);

typedef struct{
  int thread_num;
  int rw;
  int crate_num;
  int slot_num;
  int register_num;
  uint32_t data;
} frw_t;

typedef struct{
  int thread_num;
  int crate_num;
  int slot_num;
  int quiet;
} read_bundle_t;

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t pattern[16];
} load_relays_t;

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t default_ch_mask;
  int dacvalue;
} cmd_setup_chinj_t;

typedef struct{
  int thread_num;
  int crate_num;
  int slot_num;
  int dac_num;
  int dac_value; 
} cmd_load_dac_t;

#endif
