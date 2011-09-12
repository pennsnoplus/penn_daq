#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "main.h"
#include "net.h"
#include "process_packet.h"
#include "net_utils.h"
#include "daq_utils.h"

int run_macro_from_tut(char *buffer)
{
  char filename[250];
  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'f'){
        if ((words2 = strtok(NULL," ")) != NULL)
          strcpy(filename,words2);
      }else if (words[1] == 'h'){
        printsend("Usage: run_macro -f [file name]\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  if (filename != NULL)
    parse_macro(filename);
  return 0;
}

int run_macro()
{
  if (macro_cur_cmd == macro_tot_cmds){
    running_macro = 0;
    printf("macro finished!\n");
    return 0;
  }
  int result = process_control_command(macro_cmds[macro_cur_cmd]);
  if (result == 0)
    macro_cur_cmd++;
  else if (result == 999){
    printf("Problem in macro!\n");
    macro_cur_cmd = macro_tot_cmds;
  }
  return 0; 
}

int parse_macro(char *filename)
{
  FILE *macro_file;
  char long_filename[250];
  sprintf(long_filename,"macro/%s",filename);
  macro_file = fopen(long_filename,"r");
  macro_tot_cmds = 0;
  macro_cur_cmd = 0;
  while (fgets(macro_cmds[macro_tot_cmds],250,macro_file) != NULL){
    printf("new command: .%s.\n",macro_cmds[macro_tot_cmds]);
    macro_tot_cmds++;
  }
  fclose(macro_file);
  if (macro_tot_cmds > 0)
    running_macro = 1;
  printf("done reading macro\n");
  return 0; 
}

int reset_speed()
{
  megabundle_count = 0;
  return 0;
}

void unthread_and_unlock(int sbc, uint32_t crate_mask, int thread_num)
{
  if (sbc)
    sbc_lock = 0;
  int i;
  for (i=0;i<19;i++){
    if ((0x1<<i) & crate_mask)
      xl3_lock[i] = 0;
  }
  thread_done[thread_num] = 1;
}

int thread_and_lock(int sbc, uint32_t crate_mask, pthread_t **new_thread)
{
  if (sbc){
    if (sbc_lock != 0){
      // locked, cant do this right now
      return -1;
    }
    if (sbc_connected == 0 && sbc != 2){
      pt_printsend("SBC is not connected. Exiting!\n");
      return -2;
    }
  }
  int i;
  for (i=0;i<19;i++)
    if ((0x1<<i) & crate_mask){
      if (xl3_lock[i] != 0){
        // locked, cant do this right now
        return -1;
      }
      if (xl3_connected[i] == 0){
        pt_printsend("XL3 #%d is not connected. Exiting!\n",i);
        return -3;
      }
    }

  // nothings locked, so lets try spawning a thread
  *new_thread = malloc(sizeof(pthread_t));
  int thread_num = -5;
  for (i=0;i<MAX_THREADS;i++){
    if (thread_pool[i] == NULL){
      thread_pool[i] = *new_thread;
      thread_num = i;
      break;
    }
  }
  if (thread_num < 0){
    pt_printsend("All threads busy currently\n");
    return -4;
  }

  // we have a thread, so lock everything down
  if (sbc)
    sbc_lock = 1;
  for (i=0;i<19;i++)
    if ((0x1<<i) & crate_mask)
      xl3_lock[i] = 1;

  return thread_num;
}

int read_configuration_file()
{
  FILE *config_file;
  config_file = fopen(CONFIG_FILE_LOC,"r");
  int i,n = 0;
  char line_in[100][100];
  memset(DB_USERNAME,0,100);
  memset(DB_PASSWORD,0,100);
  memset(DEFAULT_SSHKEY,0,100);
  while (fscanf(config_file,"%s",line_in[n]) == 1){
    n++;
  }
  for (i=0;i<n;i++){
    char *var_name,*var_value;
    var_name = strtok(line_in[i],"=");
    if (var_name != NULL){
      var_value = strtok(NULL,"=");
      if (var_name[0] != '#' && var_value != NULL){
        if (strcmp(var_name,"NEED_TO_SWAP")==0){
          NEED_TO_SWAP = atoi(var_value);
        }else if (strcmp(var_name,"MTC_XILINX_LOCATION")==0){
          strcpy(MTC_XILINX_LOCATION,var_value);
        }else if (strcmp(var_name,"DEFAULT_SSHKEY")==0){
          strcpy(DEFAULT_SSHKEY,var_value);
        }else if (strcmp(var_name,"DB_ADDRESS")==0){
          strcpy(DB_ADDRESS,var_value);
        }else if (strcmp(var_name,"DB_PORT")==0){
          strcpy(DB_PORT,var_value);
        }else if (strcmp(var_name,"DB_USERNAME")==0){
          strcpy(DB_USERNAME,var_value);
        }else if (strcmp(var_name,"DB_PASSWORD")==0){
          strcpy(DB_PASSWORD,var_value);
        }else if (strcmp(var_name,"DB_BASE_NAME")==0){
          strcpy(DB_BASE_NAME,var_value);
        }else if (strcmp(var_name,"DB_VIEW_DOC")==0){
          strcpy(DB_VIEWDOC,var_value);
        }else if (strcmp(var_name,"MAX_PENDING_CONS")==0){
          MAX_PENDING_CONS = atoi(var_value);
        }else if (strcmp(var_name,"XL3_PORT")==0){
          XL3_PORT = atoi(var_value);
        }else if (strcmp(var_name,"SBC_PORT")==0){
          SBC_PORT = atoi(var_value);
        }else if (strcmp(var_name,"SBC_USER")==0){
          strcpy(SBC_USER,var_value);
        }else if (strcmp(var_name,"SBC_SERVER")==0){
          strcpy(SBC_SERVER,var_value);
        }else if (strcmp(var_name,"CONT_PORT")==0){
          CONT_PORT = atoi(var_value);
        }else if (strcmp(var_name,"CONT_CMD_ACK")==0){
          strcpy(CONT_CMD_ACK,var_value);
        }else if (strcmp(var_name,"CONT_CMD_BSY")==0){
          strcpy(CONT_CMD_BSY,var_value);
        }else if (strcmp(var_name,"VIEW_PORT")==0){
          VIEW_PORT = atoi(var_value);
        }else if (strcmp(var_name,"BUNDLE_PRINT")==0){
          BUNDLE_PRINT = atoi(var_value);
        }else if (strcmp(var_name,"CURRENT_LOCATION")==0){
          CURRENT_LOCATION = atoi(var_value);
        }
      }
    }
  }
  fclose(config_file);
  printf("done reading config\n");
  return 0; 
}

int set_location(char *buffer)
{
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'p'){
        printsend("location set to penn test stand\n");
        CURRENT_LOCATION = 2;
      }
      if (words[1] == 'u'){
        printsend("location set to underground\n");
        CURRENT_LOCATION = 1;
      }
      if (words[1] == 'a'){
        printsend("location set to above ground test stand\n");
        CURRENT_LOCATION = 0;
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

int kill_all_threads()
{
  int u;
  for (u=0;u<MAX_THREADS;u++){
    if (thread_pool[u] != NULL){
      pthread_cancel(*thread_pool[u]);
      free(thread_pool[u]);
    }
  }
  sbc_lock = 0;
  for (u=0;u<19;u++)
    xl3_lock[u] = 0;
  printsend("All threads DESTROYED\n");
  return 0;
}

void sigint_func(int sig) 
{
  printsend("\nBeginning shutdown\n");
  kill_all_threads();

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
  if (NEED_TO_SWAP){
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
  }
}

void SwapShortBlock(void* p, int32_t n){
  if (NEED_TO_SWAP){
    int16_t* sp = (int16_t*)p;
    int32_t i;
    for(i=0;i<n;i++){
      int16_t x = *sp;
      *sp = ((x & 0x00FF) << 8) |
        ((x & 0xFF00) >> 8) ;
      sp++;
    }
  }
}
