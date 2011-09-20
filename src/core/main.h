/** \file main.h */


#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>
#include <pthread.h>

#include "db_types.h"
#include "packet_types.h"

#define ABOVE_GROUND_TESTSTAND 0
#define UNDERGROUND 1
#define PENN_TESTSTAND 2

#define MAX_THREADS 20

#define DEF_NEED_TO_SWAP 1

#define DEF_MTC_XILINX_LOCATION "data/mtcxilinx.rbt" 

pthread_t *thread_pool[MAX_THREADS]; //!< Pool of threads to make new tests in
int thread_done[MAX_THREADS+1]; //!< Flags that a thread has finished and can be cleared

char DB_SERVER[100]; //<! holds the full database path

hware_vals_t crate_config[19][16]; //!< contains the most up to date crate configuration

FILE *ps_log_file; //!< log of anything that would go to viewer terminal
int write_log; //!< flags whether to write to log or not

int running_macro;
char macro_cmds[1000][250];
int macro_tot_cmds;
int macro_cur_cmd;

int running_final_test;
pthread_mutex_t final_test_cmd_lock; 
pthread_cond_t final_test_cmd_cv;
char final_test_cmd[1000];

int command_number[19]; //!< Numbers cmd packets sent to xl3

MultiFC multifc_buffer[19];
int multifc_buffer_full[19];

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
int SBC_PORT;
char SBC_USER[100];
char SBC_SERVER[100];
int CONT_PORT;
char CONT_CMD_ACK[100];
char CONT_CMD_BSY[100];
int VIEW_PORT;
int BUNDLE_PRINT;
int CURRENT_LOCATION;

char *PENN_DAQ_ROOT;

#endif
