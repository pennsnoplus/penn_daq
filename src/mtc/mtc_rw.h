/** \file mtc_rw.h */

#ifndef __MTC_RW_H
#define __MTC_RW_H

#include "packet_types.h"
#include "mtc_registers.h"

int do_mtc_cmd(SBC_Packet *packet);
int do_mtc_xilinx_cmd(SBC_Packet *packet);

int mtc_reg_write(uint32_t address, uint32_t data);
int mtc_reg_read(uint32_t address, uint32_t *data);

#endif
