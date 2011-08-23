#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "main.h"
#include "net_utils.h"
#include "daq_utils.h"

int set_location(char *buffer)
{
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'p'){
        printsend("location set to penn test stand\n");
        current_location = 2;
      }
      if (words[1] == 'u'){
        printsend("location set to underground\n");
        current_location = 1;
      }
      if (words[1] == 'a'){
        printsend("location set to above ground test stand\n");
        current_location = 0;
      }
      if (words[1] == 'h'){
        printsend("Usage: set_location"
            "-a (above ground) -u (under ground) -p (penn)\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }
  return 0;
}

int cleanup_threads()
{
  int i;
  for (i=0;i<MAX_THREADS;i++){
    if (thread_done[i]){
      free(thread_pool[i]);
      thread_pool[i] = NULL;
      thread_done[i] = 0;
    }
  }
  return 0;
}

void sigint_func(int sig) 
{
  printsend("\nBeginning shutdown\n");
  printsend("Closing all connections\n");
  int u;
  for(u = 0; u <= fdmax; u++){
    if(FD_ISSET(u, &main_fdset)){
      close(u);
    }
  }
  if(write_log){
    printsend("Closing log\n");
    if(ps_log_file){
      stop_logging();
    }
  }
  printsend("Killing any remaining threads\n");
  for (u=0;u<MAX_THREADS;u++){
    if (thread_pool[u] != NULL){
      pthread_cancel(*thread_pool[u]);
      free(thread_pool[u]);
    }
  }

  exit(0);
}

int start_logging(){
  if(!write_log){
    write_log = 1;
    char log_name[256] = {'\0'};  // random size, it's a pretty nice number though.
    time_t curtime = time(NULL);
    struct timeval moretime;
    gettimeofday(&moretime,0);
    struct tm *loctime = localtime(&curtime);

    strftime(log_name, 256, "%Y_%m_%d_%H_%M_%S_", loctime);
    sprintf(log_name+strlen(log_name), "%d.log", (int)moretime.tv_usec);
    ps_log_file = fopen(log_name, "a+");
    printsend( "Enabled logging\n");
    printsend( "Opened log file: %s\n", log_name);

  }
  else{
    printsend("Logging already enabled\n");
  }
  return 0;
}

int stop_logging(){
  if(write_log){
    write_log = 0;
    printsend("Disabled logging\n");
    if(ps_log_file){
      printsend("Closed log file\n");
      fclose(ps_log_file);
      system("mv *.log ./logs");
    }
    else{
      printsend("\tNo log file to close\n");
    }
  }
  else{
    printsend("Logging is already disabled\n");
  }
  return 0;
}

void SwapLongBlock(void* p, int32_t n){
#ifdef NeedToSwap
  int32_t* lp = (int32_t*)p;
  int32_t i;
  for(i=0;i<n;i++){
    int32_t x = *lp;
    *lp = (((x) & 0x000000FF) << 24) |
      (((x) & 0x0000FF00) << 8) |
      (((x) & 0x00FF0000) >> 8) |
      (((x) & 0xFF000000) >> 24);
    lp++;
  }
#endif
}

void SwapShortBlock(void* p, int32_t n){
#ifdef NeedToSwap
  int16_t* sp = (int16_t*)p;
  int32_t i;
  for(i=0;i<n;i++){
    int16_t x = *sp;
    *sp = ((x & 0x00FF) << 8) |
      ((x & 0xFF00) >> 8) ;
    sp++;
  }
#endif
}
