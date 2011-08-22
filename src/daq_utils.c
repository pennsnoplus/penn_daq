#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "main.h"
#include "net_utils.h"
#include "daq_utils.h"

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
  return 1;
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
  return 1;
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
