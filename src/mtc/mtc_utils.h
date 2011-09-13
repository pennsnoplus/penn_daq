/** \file mtc_utils.h */

#ifndef __MTC_UTILS_H
#define __MTC_UTILS_H

#include "mtc_registers.h"

int get_gt_count(uint32_t *count);
float set_gt_delay(float gtdel);
int soft_gt();
int multi_softgt(int number);

int setup_pedestals(float pulser_freq, uint32_t ped_width, 	uint32_t coarse_delay,
    uint32_t fine_delay, uint32_t ped_crate_mask, uint32_t gt_crate_mask);
int mtc_xilinx_load();
static char* getXilinxData(long *howManyBits);
int load_mtca_dacs_by_counts(uint16_t *raw_dacs);
int load_mtca_dacs(float *voltages);
int set_lockout_width(uint16_t width);
int set_gt_counter(uint32_t count);
int set_prescale(uint16_t scale);
int set_pulser_frequency(float freq);
int set_pedestal_width(uint16_t width);
int set_coarse_delay(uint16_t delay);
float set_fine_delay(float delay);
void reset_memory();

void enable_pedestal();
void disable_pedestal();
void enable_pulser();
void disable_pulser();
void unset_gt_mask(uint32_t raw_trig_types);
void set_gt_mask(uint32_t raw_trig_types);
void unset_ped_crate_mask(uint32_t crates);
void set_ped_crate_mask(uint32_t crates);
void unset_gt_crate_mask(uint32_t crates);
void set_gt_crate_mask(uint32_t crates);

/* Default setup parameters */

#define DEFAULT_GT_DELAY 150
#define DEFAULT_GT_FINE_DELAY 0
#define DEFAULT_PED_WIDTH 25

#define DEFAULT_LOCKOUT_WIDTH  400               /* in ns */
#define DEFAULT_GT_MASK        EXT8_PULSE_ASYNC
#define DEFAULT_PED_CRATE_MASK MASKALL
#define DEFAULT_GT_CRATE_MASK  MASKALL 

/* Dac load stuff */

#define MTCA_DAC_SLOPE                  (2048.0/5000.0)
#define MTCA_DAC_OFFSET                 ( + 2048 )

/* Masks */

#define MASKALL 0xFFFFFFFFUL
#define MASKNUN 0x00000000UL

#define MSK_TUB   MSK_CRATE21

/* Xilinx load stuff */

#define MAX_DATA_SIZE                   150000 // max number of allowed bits


#endif
