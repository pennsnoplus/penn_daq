/** \file daq_utils.h */

#ifndef __DAQ_UTILS_H
#define __DAQ_UTILS_H

#define CONFIG_FILE_LOC "data/config.cfg"

int thread_and_lock(int sbc, uint32_t crate_mask, pthread_t *new_thread);
int set_location(char *buffer); //!< Changes the global location variable, which is used when posting to debug database
int start_logging(); //!< Starts printing anything that would go to viewer terminal to a file
int stop_logging(); //!< Stops printing to log file
int cleanup_threads(); //!< Any thread in the pool that has flagged that its done is deleted and the pool slot is cleared
void sigint_func(int sig); //!< handles ctrl-C
void SwapLongBlock(void* p, int32_t n); //!< handles changing endianness
void SwapShortBlock(void* p, int32_t n); //!< handles changing endianness

#endif
