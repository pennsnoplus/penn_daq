/** \file xl3_registers.h */

#include <stdint.h>

#ifndef __XL3_REGISTERS_H
#define __XL3_REGISTERS_H

#define WRITE_REG	0x00000000
#define READ_REG	0x10000000
#define WRITE_MEM	0x20000000
#define READ_MEM	0x30000000
#define FEC_SEL		0x00100000

// XL2/3 REGISTERS
#define XL_RESET_R	0x02000000
#define XL_DATA_AVAIL_R 0x02000001
#define XL_CSR_R	0x02000002
#define XL_DAV_MASK_R	0x02000003
#define XL_CLOCK_R	0x02000004
#define XL_RELAY_R	0x02000005
#define XL_XLCON_R	0x02000006
#define XL_TEST_R	0x02000007
#define XL_HV_CSR_R	0x02000008
#define XL_HV_SP_R	0x02000009
#define XL_HV_VR_R	0x0200000A
#define XL_HV_CR_R	0x0200000B
#define XL_BVM_R	0x0200000C
#define XL_BVR_R	0x0200000E

// FEC REGISTERS
// Discrete
#define GENERAL_CSR_R 0x20
#define ADC_VALUE_R 0x21
#define VOLTAGE_MON_R 0x22
#define PED_ENABLE_R 0x23
#define DAC_PROG_R 0x24
#define CALDAC_PROG_R 0x25
#define FEC_HV_CSR_R 0x26
#define CMOS_PROG_LOW_R 0x2A
#define CMOS_PROG_HIGH_R 0x2B
#define CMOS_PROG(num) (0x2A + 0x1*num)
#define CMOS_LGISEL_R 0x2C
#define BOARD_ID_R 0x2D
// Sequencer
#define SEQ_OUT_CSR_R 0x80
#define SEQ_IN_CSR_R 0x84
#define CMOS_DAV_R   0x88
#define CMOS_CHIPSEL_R 0x8C
#define CMOS_CHIP_DISABLE_R 0x90
#define CMOS_DATAOUT_R 0x94
#define FIFO_READ_PTR_R 0x9c
#define FIFO_WRITE_PTR_R 0x9d
#define FIFO_DIFF_PTR_R 0x9e
// Cmos internal 
#define CMOS_AUTO_INCR_PTR(num) (0x100+0x0+0x8*num)
#define CMOS_BUSY_BIT(num) (0x100+0x1+0x8*num)
#define CMOS_MISSED_COUNT(num) (0x100+0x2+0x8*num)
#define CMOS_INTERN_TOTAL(num) (0x100+0x3+0x8*num)
#define CMOS_INTERN_TEST(num) (0x100+0x4+0x8*num)
#define CMOS_SPARE_COUNT(num) (0x100+0x5+0x8*num)
#define CMOS_ARRAY_PTR(num) (0x100+0x6+0x8*num)
#define CMOS_COUNT_INFO(num) (0x100+0x7+0x8*num)

static const uint32_t xl3_reg_addresses[14] = {XL_RESET_R, XL_DATA_AVAIL_R, XL_CSR_R, XL_DAV_MASK_R, XL_CLOCK_R, XL_RELAY_R, XL_XLCON_R, XL_TEST_R, XL_HV_CSR_R, XL_HV_SP_R, XL_HV_VR_R, XL_HV_CR_R, XL_BVM_R, XL_BVR_R};
static const char xl3_reg_names[14][50] = {"Reset","Data Available","Control Status","Data Available Mask","Clock Control","Relay","Xilinx Control","Test","HV Control Status","HV Setpoints","HV Voltage Readback","HV Current Readback","Voltage Monitoring","Low Voltage Readback"};
static const uint32_t fec_reg_addresses[28] = {GENERAL_CSR_R,ADC_VALUE_R,VOLTAGE_MON_R,PED_ENABLE_R,DAC_PROG_R,CALDAC_PROG_R,FEC_HV_CSR_R,CMOS_PROG_LOW_R,CMOS_PROG_HIGH_R,CMOS_LGISEL_R,BOARD_ID_R,SEQ_OUT_CSR_R,SEQ_IN_CSR_R,CMOS_DAV_R,CMOS_CHIPSEL_R,CMOS_CHIP_DISABLE_R,CMOS_DATAOUT_R,FIFO_READ_PTR_R,FIFO_WRITE_PTR_R,FIFO_DIFF_PTR_R,CMOS_AUTO_INCR_PTR(0),CMOS_BUSY_BIT(0),CMOS_MISSED_COUNT(0),CMOS_INTERN_TOTAL(0),CMOS_INTERN_TEST(0),CMOS_SPARE_COUNT(0),CMOS_ARRAY_PTR(0),CMOS_COUNT_INFO(0)};
static const char fec_reg_names[28][50] = {"General Control Status","ADC Value","Voltage Monitoring","Pedestal Enable","DAC Programming","Calibration DAC Programming","FEC HV Control Status","CMOS Programming Low Bits","CMOS Programming High Bits","CMOS LGI Select","Board ID","Sequencer Out Control Status","Sequencer In Control Status","CMOS Data Available","CMOS Chip Select","CMOS Chip Disable","CMOS Data Out","FIFO Read Pointer","FIFO Write Pointer","FIFO Difference Pointer","CMOS Auto Increment Pointer","CMOS Busy Bit","CMOS Missed Count","CMOS Internal Total Count","CMOS Internal Test","CMOS Spare Count","CMOS Array Pointer","CMOS Count Info"};


#endif
