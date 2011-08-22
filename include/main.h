/** \file main.h */

#ifndef __MAIN_H
#define __MAIN_H

#include <stdio.h>

#define DB_SERVER "http://localhost:5984"
#define DB_ADDRESS "localhost"
#define DB_PORT "5984"
#define DB_BASE_NAME "penndb1"
#define DB_VIEWDOC "_design/view_doc/_view"

#define MAX_PACKET_SIZE 1444
#define XL3_MAX_PAYLOAD_SIZE 1440

int current_location; //!< Where penn_daq is running (for database use)
int write_log; //!< flags whether to write to log or not
FILE *ps_log_file; //!< log of anything that would go to viewer terminal

#endif
