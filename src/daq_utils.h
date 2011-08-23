/** \file daq_utils.h */

#ifndef __DAQ_UTILS_H
#define __DAQ_UTILS_H

int set_location(char *buffer);
int start_logging();
int stop_logging();
int cleanup_threads();
void sigint_func(int sig); //!< handles ctrl-C
void SwapLongBlock(void* p, int32_t n); //!< handles changing endianness
void SwapShortBlock(void* p, int32_t n); //!< handles changing endianness

#endif
