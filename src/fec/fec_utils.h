/** \file fec_utils.h */

#ifndef __FEC_UTILS_H
#define __FEC_UTILS_H

#define FEC_CSR_CRATE_OFFSET 11
#define MAX_FEC_COMMANDS 60000

// hv
#define HV_CSR_CLK 0x1
#define HV_CSR_DATIN 0x2
#define HV_CSR_LOAD 0x4
#define HV_CSR_DATOUT 0x8

void dump_pmt_verbose(int n, uint32_t *pmt_buf, char* msg_buf);
int get_cmos_total_count(int crate, int slot, uint32_t *total_count, fd_set *thread_fdset);
int loadsDac(uint32_t dac_num, uint32_t dac_value, int crate_num, int slot_num, fd_set *thread_fdset);
int multi_loadsDac(int num_dacs, uint32_t *dac_nums, uint32_t *dac_values, int crate_num, int slot_num, fd_set *thread_fdset);
int32_t read_out_bundles(int crate, int slot, int limit, uint32_t *pmt_buf, fd_set *thread_fdset);
int fec_load_crate_addr(int crate, uint16_t slot_mask, fd_set *thread_fdset);
int set_crate_pedestals(int crate, uint16_t slot_mask, uint32_t pattern, fd_set *thread_fdset);

struct channel_params{
  int hi_balanced;
  int low_balanced;
  short int test_status;
  short int low_gain_balance;
  short int high_gain_balance;
};

struct cell {
  int16_t per_cell;
  int cellno;
  double qlxbar, qlxrms;
  double qhlbar, qhlrms;
  double qhsbar, qhsrms;
  double tacbar, tacrms;
};

struct pedestal {
  int16_t channelnumber;
  int16_t per_channel;
  struct cell thiscell[16];
};


#endif
