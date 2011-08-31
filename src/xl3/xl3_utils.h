/** \file xl3_utils.h */

#ifndef __XL3_UTILS_H
#define __XL3_UTILS_H

#define INIT_MODE 0x1
#define NORMAL_MODE 0x2

int deselect_fecs(int crate, fd_set *thread_fdset); //!< clears the address register in the VHDL
int update_crate_config(int crate, uint16_t slot_mask, fd_set *thread_fdset); //!< Gets the ids of the mbs and dbs in the crate and updates the global variable
int change_mode(int mode, uint16_t davail_mask, int xl3num, fd_set *thread_fdset);

#endif
