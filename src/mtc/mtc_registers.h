/** \file mtc_registers.h */

#ifndef __MTC_REGISTERS_H
#define __MTC_REGISTERS_H

#include <stdint.h>
#include <string.h>

// define MTC register space
#define MTCRegAddressBase  (0x00007000)
#define MTCRegAddressMod   (0x29)
#define MTCRegAddressSpace (0x01)
#define MTCMemAddressBase  (0x03800000)
#define MTCMemAddressMod   (0x09)
#define MTCMemAddressSpace (0x02)

#define MTCControlReg (0x0)
#define MTCSerialReg  (0x4)
#define MTCDacCntReg (0x8)
#define MTCSoftGTReg (0xC)
#define MTCPedWidthReg (0x10)
#define MTCCoarseDelayReg (0x14)
#define MTCFineDelayReg (0x18)
#define MTCThresModReg (0x1C)
#define MTCPmskReg (0x20)
#define MTCScaleReg (0x24)
#define MTCBwrAddOutReg (0x28)
#define MTCBbaReg (0x2C)
#define MTCGtLockReg (0x30)
#define MTCMaskReg (0x34)
#define MTCXilProgReg (0x38)
#define MTCGmskReg (0x3C)
#define MTCOcGtReg (0x80)
#define MTCC50_0_31Reg (0x84)
#define MTCC50_32_42Reg (0x88)
#define MTCC10_0_31Reg (0x8C)
#define MTCC10_32_52Reg (0x90)

//////////////////////////////
// REGISTER VALUES ///////////
//////////////////////////////

/* control register */

#define PED_EN     0x00000001UL
#define PULSE_EN   0x00000002UL
#define LOAD_ENPR  0x00000004UL
#define LOAD_ENPS  0x00000008UL
#define LOAD_ENPW  0x00000010UL
#define LOAD_ENLK  0x00000020UL
#define ASYNC_EN   0x00000040UL
#define RESYNC_EN  0x00000080UL
#define TESTGT     0x00000100UL
#define TEST50     0x00000200UL
#define TEST10     0x00000400UL
#define LOAD_ENGT  0x00000800UL
#define LOAD_EN50  0x00001000UL
#define LOAD_EN10  0x00002000UL
#define TESTMEM1   0x00004000UL
#define TESTMEM2   0x00008000UL
#define FIFO_RESET 0x00010000UL

/* serial register */           

#define SEN        0x00000001UL
#define SERDAT     0x00000002UL
#define SHFTCLKGT  0x00000004UL
#define SHFTCLK50  0x00000008UL
#define SHFTCLK10  0x00000010UL
#define SHFTCLKPS  0x00000020UL

/* Xilinx register */

#define MTC_CLOCK                       0x00000002UL //sets CCLK high
#define MTC_SET_DP                      0x00000004UL //sets DP high
#define MTC_DRIVE_DP                    0x00000008UL //enables DP buffer
#define MTC_DONE_PROG                   0x00000010UL //DP readback

/* DAC_control register */

#define TUB_SDATA  0x00000400UL
#define TUB_SCLK   0x00000800UL
#define TUB_SLATCH 0x00001000UL
#define DACSEL     0x00004000UL 
#define DACCLK     0x00008000UL

/* Global Trigger Mask */

#define NHIT100LO        0x00000001UL
#define NHIT100MED       0x00000002UL
#define NHIT100HI        0x00000004UL
#define NHIT20           0x00000008UL
#define NHIT20LB         0x00000010UL
#define ESUMLO           0x00000020UL
#define ESUMHI           0x00000040UL
#define OWLN             0x00000080UL
#define OWLELO           0x00000100UL
#define OWLEHI           0x00000200UL
#define PULSE_GT         0x00000400UL
#define PRESCALE         0x00000800UL
#define PEDESTAL         0x00001000UL
#define PONG             0x00002000UL
#define SYNC             0x00004000UL
#define EXT_ASYNC        0x00008000UL
#define EXT2             0x00010000UL 
#define EXT3             0x00020000UL
#define EXT4             0x00040000UL
#define EXT5             0x00080000UL
#define EXT6             0x00100000UL
#define EXT7             0x00200000UL
#define EXT8_PULSE_ASYNC 0x00400000UL
#define SPECIAL_RAW      0x00800000UL 
#define NCD              0x01000000UL
#define SOFT_GT          0x02000000UL

/* Crate Masks */

#define MSK_CRATE1  0x00000001UL
#define MSK_CRATE2  0x00000002UL
#define MSK_CRATE3  0x00000008UL
#define MSK_CRATE4  0x00000004UL
#define MSK_CRATE5  0x00000010UL
#define MSK_CRATE6  0x00000020UL
#define MSK_CRATE7  0x00000040UL
#define MSK_CRATE8  0x00000080UL
#define MSK_CRATE9  0x00000100UL
#define MSK_CRATE10 0x00000200UL
#define MSK_CRATE11 0x00000400UL
#define MSK_CRATE12 0x00000800UL
#define MSK_CRATE13 0x00001000UL
#define MSK_CRATE14 0x00002000UL
#define MSK_CRATE15 0x00004000UL
#define MSK_CRATE16 0x00008000UL
#define MSK_CRATE17 0x00010000UL
#define MSK_CRATE18 0x00020000UL
#define MSK_CRATE19 0x00040000UL
#define MSK_CRATE20 0x00080000UL
#define MSK_CRATE21 0x00100000UL        // the TUB!
#define MSK_CRATE22 0x00200000UL
#define MSK_CRATE23 0x00400000UL
#define MSK_CRATE24 0x00800000UL
#define MSK_CRATE25 0x01000000UL
#define MSK_TUB     MSK_CRATE21         // everyone's favorite board!  

/* Threshold Monitoring */

#define TMON_NHIT100LO  0x00020000UL
#define TMON_NHIT100MED 0x00000000UL
#define TMON_NHIT100HI  0x00060000UL
#define TMON_NHIT20     0x00040000UL
#define TMON_NHIT20LB   0x000a0000UL
#define TMON_ESUMLO     0x00080000UL
#define TMON_ESUMHI     0x000e0000UL
#define TMON_OWLELO     0x00120000UL
#define TMON_OWLEHI     0x00100000UL
#define TMON_OWLN       0x000c0000UL


static const uint32_t mtc_reg_addresses[21] = {MTCControlReg,MTCSerialReg,MTCDacCntReg,MTCSoftGTReg,MTCPedWidthReg,MTCCoarseDelayReg,MTCFineDelayReg,MTCThresModReg,MTCPmskReg,MTCScaleReg,MTCBwrAddOutReg,MTCBbaReg,MTCGtLockReg,MTCMaskReg,MTCXilProgReg,MTCGmskReg,MTCOcGtReg,MTCC50_0_31Reg,MTCC50_32_42Reg,MTCC10_0_31Reg,MTCC10_32_52Reg};
static const char mtc_reg_names[21][50] = {"Control","Serial","DAC Control","Soft GT","Pedestal Width","Coarse GT Delay","Fine GT Delay","Threshold Monitor","Pedestal Crate Mask","Prescale Value","Next Write Pointer","Memory Access Pointer","Lockout Width","GT Mask","Xilinx Program","GT Crate Mask","GT Count","Lower 50MHz Count","Upper 50MHz Count","Lower 10MHz Count","Upper 10MHz Count"};

#endif
