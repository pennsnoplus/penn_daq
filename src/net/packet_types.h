/** \file packet_types.h */

#ifndef __PACKET_TYPES_H
#define __PACKET_TYPES_H

#include <stdint.h>

#include "db_types.h"

#define XL3_MAX_PAYLOAD_SIZE 1440 //!< size of the payload in XL3_Packet
#define XL3_HEADER_SIZE 4 //!< bytes in the XL3_Packet header
#define MAX_PACKET_SIZE 1444 //!< Maximum size of packet that lwip expects

typedef struct {
    uint16_t packet_num;
    uint8_t packet_type;
    uint8_t num_bundles;
} XL3_CommandHeader;

typedef struct {
    XL3_CommandHeader cmdHeader;
    char payload[XL3_MAX_PAYLOAD_SIZE];
} XL3_Packet;

#define kSBC_MaxPayloadSizeBytes 1024*400
#define kSBC_MaxMessageSizeBytes    256

typedef struct {
    uint32_t destination;    /*should be kSBC_Command*/
    uint32_t cmdID;
    uint32_t numberBytesinPayload;
} SBC_CommandHeader;

typedef struct {
    uint32_t numBytes;                //filled in automatically
    SBC_CommandHeader cmdHeader;
    char message[kSBC_MaxMessageSizeBytes];
    char payload[kSBC_MaxPayloadSizeBytes];
} SBC_Packet;

/*! \name xl3_send_packet_type
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

/*! \name xl3_recv_packet_types
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

typedef struct {
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} PMTBundle;


typedef struct {
    uint32_t cmd_num;
    uint16_t packet_num;
    uint8_t flags;
    uint32_t address;
    uint32_t data;
} FECCommand;

#define MAX_ACKS_SIZE 80

typedef struct {
    uint32_t howmany;
    FECCommand cmd[MAX_ACKS_SIZE];
} MultiFC;

// packet arguments and results

typedef struct{
  uint32_t slot_mask;
} fec_test_args_t;

typedef struct{
  uint32_t error_flag;
  uint32_t discrete_reg_errors[16]; 
  uint32_t cmos_test_reg_errors[16];
} fec_test_results_t;

typedef struct{
  uint32_t mb_num;
  uint32_t xilinx_load;
  uint32_t hv_reset;
  uint32_t slot_mask;
  uint32_t ctc_delay;
  uint32_t shift_reg_only;
} crate_init_args_t;

typedef struct{
  uint32_t error_flags;
  hware_vals_t hware_vals[16];
} crate_init_results_t;





/*! \name sbc_send_packet_type
 *  Types of packets that will be sent to the sbc from the xl3
 */
//@{
#define MTC_XILINX_ID           (0x1)
#define MTC_READ_ID             (0x2)
#define MTC_WRITE_ID            (0x3)
//@}

typedef struct {
    int32_t baseAddress;
    int32_t addressModifier;
    int32_t programRegOffset;
    uint32_t errorCode;
    int32_t fileSize;
} SNOMtc_XilinxLoadStruct;

typedef struct {
    uint32_t address;        /*first address*/
    uint32_t addressModifier;
    uint32_t addressSpace;
    uint32_t unitSize;        /*1,2,or 4*/
    uint32_t errorCode;    /*filled on return*/
    uint32_t numItems;        /*number of items to read*/
} SBC_VmeReadBlockStruct;

typedef struct {
    uint32_t address;        /*first address*/
    uint32_t addressModifier;
    uint32_t addressSpace;
    uint32_t unitSize;        /*1,2,or 4*/
    uint32_t errorCode;    /*filled on return*/
    uint32_t numItems;        /*number Items of data to follow*/
    /*followed by the requested data, number of items from above*/
} SBC_VmeWriteBlockStruct;

#endif
