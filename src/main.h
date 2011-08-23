/** \file main.h */


#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>
#include <pthread.h>

#include "db.h"

#define DB_ADDRESS "localhost"
#define DB_PORT "5984"
#define DB_USERNAME ""
#define DB_PASSWORD ""
#define DB_BASE_NAME "penndb1"
#define DB_VIEWDOC "_design/view_doc/_view"

char DB_SERVER[100];

#define MAX_PACKET_SIZE 1444
#define XL3_MAX_PAYLOAD_SIZE 1440

#define NeedToSwap

#define MAX_THREADS 20

hware_vals_t crate_config[19][16];

pthread_t *thread_pool[MAX_THREADS];
int thread_done[MAX_THREADS];

int current_location; //!< Where penn_daq is running (for database use)
int write_log; //!< flags whether to write to log or not
FILE *ps_log_file; //!< log of anything that would go to viewer terminal

int command_number;


#endif
