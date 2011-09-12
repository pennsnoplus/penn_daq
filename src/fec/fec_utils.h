/** \file fec_utils.h */

#ifndef __FEC_UTILS_H
#define __FEC_UTILS_H

#define FEC_CSR_CRATE_OFFSET 11
#define MAX_FEC_COMMANDS 60000

int32_t read_out_bundles(int crate, int slot, int limit, uint32_t *pmt_buf, fd_set *thread_fdset);
int fec_load_crate_addr(int crate, uint16_t slot_mask, fd_set *thread_fdset);
int set_crate_pedestals(int crate, uint16_t slot_mask, uint32_t pattern, fd_set *thread_fdset);

#endif
