/** \file crate_cbal.h */

#ifndef __CRATE_CBAL_H
#define __CRATE_CBAL_H

#include <stdint.h>

#include "fec_utils.h"

typedef struct{
  int thread_num;
  int crate_num;
  uint16_t slot_mask;
  uint32_t pattern;
  int update_db;
  int final_test;
  char ft_ids[16][50];
} crate_cbal_t;

int crate_cbal(char *buffer);
void *pt_crate_cbal(void *args);

#define PED_TEST_TAKEN 0x1
#define PED_CH_HAS_PEDESTALS 0x2
#define PED_RMS_TEST_PASSED 0x4
#define PED_PED_WITHIN_RANGE 0x8
#define PED_DATA_NO_ENABLE 0x10
#define PED_TOO_FEW_PER_CELL 0x20

struct PMTBundle{
    unsigned int CdID;
    unsigned int ChID;
    unsigned int GtID;
    unsigned int CeID;
    unsigned int ErID;
    unsigned int ADC_Qlx;
    unsigned int ADC_Qhs;
    unsigned int ADC_Qhl;
    unsigned int ADC_TAC;
    unsigned long Word1;
    unsigned long Word2;
    unsigned long Word3;
};

typedef struct {
    unsigned short CrateID;
    unsigned short BoardID;
    unsigned short ChannelID;
    unsigned short CMOSCellID;
    unsigned long  GlobalTriggerID;
    unsigned short GlobalTriggerID2;
    unsigned short ADC_Qlx;
    unsigned short ADC_Qhs;
    unsigned short ADC_Qhl;
    unsigned short ADC_TAC;
    unsigned short CGT_ES16;
    unsigned short CGT_ES24;
    unsigned short Missed_Count;
    unsigned short NC_CC;
    unsigned short LGI_Select;
    unsigned short CMOS_ES16;
} FEC32PMTBundle;

int get_pedestal(struct pedestal *pedestals, struct channel_params *chan_params, uint32_t *pmt_buf, int crate, int slot, uint32_t pattern, fd_set *thread_fdset);
FEC32PMTBundle MakeBundleFromData(uint32_t *buffer);

#endif
