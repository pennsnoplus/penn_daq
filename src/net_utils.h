/** \file net_utils.h */

#ifndef __NET_UTILS_H
#define __NET_UTILS_H

#include <sys/socket.h>
#include <pthread.h>

#include "main.h"

#define XL3_PORT 44601
#define SBC_PORT 44630
#define CONT_PORT 44600
#define VIEW_PORT 44599

#define MAX_XL3_CON 19
#define MAX_VIEW_CON 3

#define MAX_PENDING_CONS 20

#define SBC_USER "daq"
#define SBC_SERVER "10.0.0.30"

#define CONT_CMD_ACK "_._"
#define CONT_CMD_BSY "_!_"

/*! \name packet_type
 *  Types of packets that will be received by the xl3 from the daq
 */
//@{
// ML403 Level Tasks
#define DAQ_QUIT_ID               (0x00) //!< disconnect from the daq
#define PONG_ID                   (0x01) //!< daq is alive
#define CHANGE_MODE_ID            (0x02) //!< change to INIT or NORMAL mode
#define STATE_MACHINE_RESET_ID    (0x03) //!< reset the state machine
#define DEBUGGING_MODE_ID         (0x04) //!< turn on verbose printout
// XL3/FEC Level Tasks
#define FAST_CMD_ID               (0x20) //!< do one command in recv callback, immediately respond with same packet (daq numbered)
#define MULTI_FAST_CMD_ID         (0x21) //!< do multiple commands in recv callback, immediately respond with same packet (daq numbered)
#define QUEUE_CMDS_ID             (0x22) //!< add multiple cmds to cmd_queue, done at xl3's liesure, respond in cmd_ack packets (xl3 numbered)
#define CRATE_INIT_ID             (0x23) //!< one of the 17 packets containing the crate settings info from the database 
#define FEC_LOAD_CRATE_ADD_ID     (0x24) //!< load crate address into FEC general csr
#define SET_CRATE_PEDESTALS_ID    (0x25) //!< Sets the pedestal enables on all connected fecs either on or off
#define BUILD_CRATE_CONFIG_ID     (0x26) //!< Updates the xl3's knowledge of the mb id's and db id's
#define LOADSDAC_ID               (0x27) //!< Load a single fec dac
#define MULTI_LOADSDAC_ID         (0x28) //!< Load multiple dacs
#define DESELECT_FECS_ID          (0x29) //!< deselect all fecs in vhdl
#define READ_PEDESTALS_ID         (0x2A) //!< queue any number of memory reads into cmd queue
#define LOADTACBITS_ID            (0x2B) //!< loads tac bits on fecs
#define RESET_FIFOS_ID            (0x2C) //!< resets all the fec fifos
#define READ_LOCAL_VOLTAGE_ID     (0x2D) //!< read a single voltage on XL3 
#define CHECK_TOTAL_COUNT_ID      (0x2E) //!< readout cmos total count register 
// HV Tasks
#define SET_HV_RELAYS_ID          (0x40) //!< turns on/off hv relays
#define GET_HV_STATUS_ID          (0x41) //!< checks voltage and current readback
#define HV_READBACK_ID            (0x42) //!< reads voltage and current 
#define READ_PMT_CURRENT_ID       (0x43) //!< reads pmt current from FEC hv csr 
#define SETUP_CHARGE_INJ_ID       (0x44) //!< setup charge injection in FEC hv csr
// Tests
#define FEC_TEST_ID               (0x60) //!< check read/write to FEC registers
#define MEM_TEST_ID               (0x61) //!< check read/write to FEC ram, address lines
#define VMON_ID                   (0x62) //!< reads FEC voltages
#define BOARD_ID_READ_ID          (0x63) //!< reads id of fec,dbs,hvc
#define ZDISC_ID                  (0x64) //!< zeroes discriminators
#define CALD_TEST_ID              (0x65) //!< checks adcs with calibration dac
#define SLOT_NOISE_RATE_ID        (0x66) //!< check the noise rate in a slot      
//@}


/*! \name packet_types
 * possible cmdID's for packets sent by XL3 (to the DAQ)
 */
//@{
#define CALD_RESPONSE_ID        (0xAA) // response from cald_test, contains dac ticks and adc values 
#define PING_ID                 (0xBB) // check if daq is still connected
#define MEGA_BUNDLE_ID          (0xCC) // sending data, numbered by xl3
#define CMD_ACK_ID              (0xDD) // sending results of a cmd from the cmd queue, numbered by xl3
#define MESSAGE_ID              (0xEE) // sending a message, numbered by xl3 if a string, numbered by daq if an echod packet
#define ERROR_ID                (0xFE) // info on what error xl3 current has
#define SCREWED_ID              (0xFF) // info on what FEC is screwed
//@}


// listening fds
int listen_cont_fd;
int listen_view_fd;
int listen_xl3_fd[MAX_XL3_CON];

// read/write fds
int rw_cont_fd;
int rw_view_fd[MAX_VIEW_CON];
int rw_xl3_fd[MAX_XL3_CON];
int rw_sbc_fd;


//fdsets
fd_set main_fdset;
fd_set main_readable_fdset;
fd_set main_writeable_fdset;
fd_set new_connection_fdset;
fd_set view_fdset;
fd_set cont_fdset;
fd_set xl3_fdset;
int fdmax;
pthread_mutex_t main_fdset_lock;

// locks
int sbc_lock;
int xl3_lock[MAX_XL3_CON];

// connection flag
int sbc_connected;
int cont_connected;
int views_connected;
int xl3_connected[MAX_XL3_CON];

char buffer[MAX_PACKET_SIZE];

char printsend_buffer[10000];
pthread_mutex_t printsend_buffer_lock;

void setup_sockets();
int bind_socket(char *host, int port);

void read_socket(int fd);
int accept_connection(int fd);

int read_xl3_data(int fd);
int read_control_command(int fd);
int read_viewer_data(int fd);

int process_control_command(char *buffer);

int print_connected();
void *get_in_addr(struct sockaddr *sa);
int printsend(char *fmt, ... );
int pt_printsend(char *fmt, ...);
int send_queued_msgs();
#endif
