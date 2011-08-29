/** \file db_types.h */

#ifndef __DB_TYPES_H
#define __DB_TYPES_H

#include <stdint.h>

typedef struct {
  uint16_t mb_id;
  uint16_t dc_id[4];
} hware_vals_t;

typedef struct {
    /* index definitions [0=ch0-3; 1=ch4-7; 2=ch8-11, etc] */
    unsigned char     rmp[8];    // back edge timing ramp    
    unsigned char     rmpup[8];  // front edge timing ramp    
    unsigned char     vsi[8];    // short integrate voltage     
    unsigned char     vli[8];    // long integrate voltage
} tdisc_t;

typedef struct {
    /* the folowing are motherboard wide constants */
    unsigned char     vmax;           // upper TAC reference voltage
    unsigned char     tacref;         // lower TAC reference voltage
    unsigned char     isetm[2];       // primary   timing current [0= tac0; 1= tac1]
    unsigned char     iseta[2];       // secondary timing current [0= tac0; 1= tac1]
    /* there is one unsigned char of TAC bits for each channel */
    unsigned char     tac_shift[32];  // TAC shift register load bits (see 
    // loadcmosshift.c for details)
    // [channel 0 to 31]      
} tcmos_t;

/* CMOS shift register 100nsec trigger setup */
typedef struct {
    unsigned char      mask[32];   
    unsigned char      tdelay[32];    // tr100 width (see loadcmosshift.c for details)
    // [channel 0 to 31], only bits 0 to 6 defined
} tr100_t;

/* CMOS shift register 20nsec trigger setup */
typedef struct {
    unsigned char      mask[32];    
    unsigned char      twidth[32];    //tr20 width (see loadcmosshift.c for details)
    // [channel 0 to 31], only bits 0 to 6 defined
    unsigned char      tdelay[32];    //tr20 delay (see loadcmosshift.c for details)
    // [channel 0 to 31], only buts 0 to 4 defined
} tr20_t;

typedef struct {
    uint16_t mb_id;
    uint16_t dc_id[4];
    unsigned char vbal[2][32];
    unsigned char vthr[32];
    tdisc_t tdisc;
    tcmos_t tcmos;
    unsigned char vint;       // integrator output voltage 
    unsigned char hvref;     // MB control voltage 
    tr100_t tr100;
    tr20_t tr20;
    uint16_t scmos[32];     // remaining 10 bits (see loadcmosshift.c for 
    uint32_t  disable_mask;
} mb_t;

typedef struct {
    uint16_t lockout_width;
    uint16_t pedestal_width;
    uint16_t nhit100_lo_prescale;
    uint32_t pulser_period;
    uint32_t low10Mhz_clock;
    uint32_t high10Mhz_clock;
    float fine_slope;
    float min_delay_offset;
    uint16_t coarse_delay;
    uint16_t fine_delay;
    uint32_t gt_mask;
    uint32_t gt_crate_mask;
    uint32_t ped_crate_mask;
    uint32_t control_mask;
}mtcd_t;

typedef struct{
    char id[20];
    uint16_t threshold;
    uint16_t mv_per_adc;
    uint16_t mv_per_hit;
    uint16_t dc_offset;
}trigger_t;

typedef struct {
    trigger_t triggers[14];
    uint16_t crate_status[6];
    uint16_t retriggers[6];
}mtca_t;

typedef struct {
    mtcd_t mtcd;
    mtca_t mtca;
    uint32_t tub_bits;
}mtc_t;

#endif
