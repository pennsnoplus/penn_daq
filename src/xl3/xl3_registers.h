/** \file xl3_registers.h */

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
// hv
#define HV_CSR_CLK 0x1
#define HV_CSR_DATIN 0x2
#define HV_CSR_LOAD 0x4
#define HV_CSR_DATOUT 0x8

#endif
