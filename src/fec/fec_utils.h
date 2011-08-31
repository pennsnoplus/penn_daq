/** \file fec_utils.h */

#ifndef __FEC_UTILS_H
#define __FEC_UTILS_H

#define FEC_CSR_CRATE_OFFSET 11

int fec_load_crate_addr(int crate, uint16_t slot_mask, fd_set *thread_fdset);
int set_crate_pedestals(int crate, uint16_t slot_mask, uint32_t pattern, fd_set *thread_fdset);

#endif
