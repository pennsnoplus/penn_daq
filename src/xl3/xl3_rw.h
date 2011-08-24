/** \file xl3_rw.h */

#ifndef __XL3_RW_H
#define __XL3_RW_H

/*! \brief Sends a command packet to xl3 and gets a response
 *
 *  Only monitors the one xl3 that it is sending a packet to.
 *  It waits to get the same packet type back. Any message_id
 *  packet it gets it will print to the screen; anything else
 *  will cause an error. Expects to be run from a thread that
 *  has already locked off the necessary sockets.
 */
int do_xl3_cmd(XL3_Packet *packet,int xl3num, fd_set *fd_set);

/*! \brief Sends a command packet to xl3 and immediately returns
 *
 *  Sends a packet and then returns as soon as the write finishes
 *  successfully. Expects to be run from a thread that has already
 *  locked off the necessary sockets.
 */
int do_xl3_cmd_no_response(XL3_Packet *packet,int xl3num);

#endif
