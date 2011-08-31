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
int do_xl3_cmd(XL3_Packet *packet,int xl3num, fd_set *thread_fdset);

/*! \brief Sends a command packet to xl3 and immediately returns
 *
 *  Sends a packet and then returns as soon as the write finishes
 *  successfully. Expects to be run from a thread that has already
 *  locked off the necessary sockets.
 */
int do_xl3_cmd_no_response(XL3_Packet *packet,int xl3num);

/*! \brief Does a single register read or write
 *
 *  Uses FAST_CMD_ID, meaning it does the cmd in the recv callback
 *  and does not queue it. Calls do_xl3_cmd and then puts the result
 *  in the result pointer. Returns any flags from the xl3
 */
int xl3_rw(uint32_t address, uint32_t data, uint32_t *result, int crate_num, fd_set *thread_fdset);

/*! \brief Reads cmd result packets from xl3 until it gets the one it wants
 * 
 *  After queuing up FECCommands, you can call this function to recv
 *  and store the results of every test until it reads up to the command
 *  number that you specify. Any extra command results that it reads are
 *  stored in a global for the next time you call this function.
 */ 
int wait_for_multifc_results(int num_cmds, int packet_num, int xl3num, uint32_t *buffer, fd_set *thread_fdset);

#endif
