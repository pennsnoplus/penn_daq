/** \file fec_cmds.h */

#ifndef __FEC_CMDS_H
#define __FEC_CMDS_H

int read_bundle(char *buffer);
void *pt_read_bundle(void* args);
int load_relays(char *buffer);
void *pt_load_relays(void* args);

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

#endif
