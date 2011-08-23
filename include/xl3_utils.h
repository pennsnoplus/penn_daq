/** \file xl3_utils.h */

#ifndef __XL3_UTILS_H
#define __XL3_UTILS_H

int deselect_fecs(int crate);
int update_crate_config(int crate, uint16_t slot_mask);
int debugging_mode(char *buffer, int onoff);
void *pt_debugging_mode(void *args);
int change_mode(char *buffer);
void *pt_change_mode(void* args);

#endif
