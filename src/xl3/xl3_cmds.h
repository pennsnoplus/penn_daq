/** \file xl3_cmds.h */

#ifndef __XL3_CMDS_H
#define __XL3_CMDS_H

int debugging_mode(char *buffer, int onoff);
void *pt_debugging_mode(void *args);
int change_mode(char *buffer);
void *pt_change_mode(void* args);
int sm_reset(char *buffer);
void *pt_sm_reset(void* args);
int cmd_xl3_rw(char *buffer);
void *pt_cmd_xl3_rw(void* args);
int xl3_queue_rw(char *buffer);
void *pt_xl3_queue_rw(void* args);

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

#endif