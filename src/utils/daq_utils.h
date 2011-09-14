/** \file daq_utils.h */

#ifndef __DAQ_UTILS_H
#define __DAQ_UTILS_H

#define CONFIG_FILE_LOC "data/config.cfg"

int read_from_tut(char *result);
int run_macro_from_tut(char *buffer);
int run_macro();
int parse_macro(char *filename);
int reset_speed();
void unthread_and_unlock(int sbc, uint32_t crate_mask, int thread_num);
int thread_and_lock(int sbc, uint32_t crate_mask, pthread_t **new_thread);
int set_location(char *buffer); //!< Changes the global location variable, which is used when posting to debug database
int start_logging(); //!< Starts printing anything that would go to viewer terminal to a file
int stop_logging(); //!< Stops printing to log file
int cleanup_threads(); //!< Any thread in the pool that has flagged that its done is deleted and the pool slot is cleared
int kill_all_threads();
void sigint_func(int sig); //!< handles ctrl-C
void SwapLongBlock(void* p, int32_t n); //!< handles changing endianness
void SwapShortBlock(void* p, int32_t n); //!< handles changing endianness
uint32_t sGetBits(uint32_t value, uint32_t bit_start, uint32_t num_bits);

#endif
