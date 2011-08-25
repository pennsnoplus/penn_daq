/** \file mtc_registers.h */

#ifndef __MTC_REGISTERS_H
#define __MTC_REGISTERS_H

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

#endif
