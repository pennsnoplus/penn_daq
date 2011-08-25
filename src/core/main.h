/** \file main.h */


#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>
#include <pthread.h>

#include "db.h"

#define MAX_THREADS 20

#define NeedToSwap

#define MTC_XILINX_LOCATION "data/mtcxilinx.rbt" 

pthread_t *thread_pool[MAX_THREADS]; //!< Pool of threads to make new tests in
int thread_done[MAX_THREADS]; //!< Flags that a thread has finished and can be cleared

char DB_SERVER[100]; //<! holds the full database path

hware_vals_t crate_config[19][16]; //!< contains the most up to date crate configuration

FILE *ps_log_file; //!< log of anything that would go to viewer terminal
int write_log; //!< flags whether to write to log or not
int current_location; //!< Where penn_daq is running (for database use)

int command_number; //!< Numbers cmd packets sent to xl3

#endif
