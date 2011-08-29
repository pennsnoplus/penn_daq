/** \file main.h */


#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>
#include <pthread.h>

#include "db.h"

#define MAX_THREADS 20

#define DEF_NEED_TO_SWAP 1

#define DEF_MTC_XILINX_LOCATION "data/mtcxilinx.rbt" 

pthread_t *thread_pool[MAX_THREADS]; //!< Pool of threads to make new tests in
int thread_done[MAX_THREADS]; //!< Flags that a thread has finished and can be cleared

char DB_SERVER[100]; //<! holds the full database path

hware_vals_t crate_config[19][16]; //!< contains the most up to date crate configuration

FILE *ps_log_file; //!< log of anything that would go to viewer terminal
int write_log; //!< flags whether to write to log or not
int current_location; //!< Where penn_daq is running (for database use)

int command_number; //!< Numbers cmd packets sent to xl3

// configuration crap
int NEED_TO_SWAP;
char MTC_XILINX_LOCATION[100];
char DEFAULT_SSHKEY[100];
char DB_ADDRESS[100];
char DB_PORT[100];
char DB_USERNAME[100];
char DB_PASSWORD[100];
char DB_BASE_NAME[100];
char DB_VIEWDOC[100];
int MAX_PENDING_CONS;
int XL3_PORT;
int MAX_XL3_CON;
int SBC_PORT;
char SBC_USER[100];
char SBC_SERVER[100];
int CONT_PORT;
char CONT_CMD_ACK[100];
char CONT_CMD_BSY[100];
int VIEW_PORT;
int XL3_MAX_PAYLOAD_SIZE;
int XL3_HEADER_SIZE;
int MAX_PACKET_SIZE;

#endif
