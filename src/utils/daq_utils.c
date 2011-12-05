#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "xl3_registers.h"
#include "mtc_registers.h"

#include "main.h"
#include "net.h"
#include "process_packet.h"
#include "net_utils.h"
#include "daq_utils.h"

int print_help(char *buffer)
{
  int which = 0;
  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (strncmp(words,"xl3_registers",13) == 0){
      which = 1;
    }else if (strncmp(words,"fec_registers",13) == 0){
      which = 2;
    }else if (strncmp(words,"mtc_registers",13) == 0){
      which = 3;
    }else if (strncmp(words,"xl3",3) == 0){
      which = 4;
    }else if (strncmp(words,"fec",3) == 0){
      which = 5;
    }else if (strncmp(words,"mtc",3) == 0){
      which = 6;
    }else if (strncmp(words,"daq",3) == 0){
      which = 7;
    }else if (strncmp(words,"tests",4) == 0){
      which = 8;
    }else if (strncmp(words,"all",3) == 0){
      which = 9;
    }
    words = strtok(NULL," ");
  }

  int i,j;
  if (which == 0){
    pt_printsend("Type \"help topic\" to get more help about \"topic\"\n");
    pt_printsend("Topics:\n");
    pt_printsend("all\t\t\tView a list of all commands\n");
    pt_printsend("xl3\t\t\tView a list of xl3 commands\n");
    pt_printsend("fec\t\t\tView a list of fec commands\n");
    pt_printsend("mtc\t\t\tView a list of mtc commands\n");
    pt_printsend("daq\t\t\tView a list of daq commands\n");
    pt_printsend("tests\t\t\tView a list of tests\n");
    pt_printsend("xl3_registers\t\tView a list of register numbers and addresses\n");
    pt_printsend("fec_registers\t\tView a list of register numbers and addresses\n");
    pt_printsend("mtc_registers\t\tView a list of register numbers and addresses\n");
  }
  if (which == 1){
    pt_printsend("XL3 Registers:\n");
    for (i=0;i<14;i++)
      pt_printsend("%2d: (%08x) %s\n",i,xl3_reg_addresses[i],xl3_reg_names[i]);
  }
  if (which == 2){
    pt_printsend("FEC Registers:\n");
    for (i=0;i<20;i++)
      pt_printsend("%2d: (%08x) %s\n",i,fec_reg_addresses[i],fec_reg_names[i]);
    for (i=0;i<8;i++)
      pt_printsend("%3d - %3d: (%08x + 0x8*num) %s\n",20+i*32,20+i*32+31,fec_reg_addresses[20+i],fec_reg_names[20+i]);
  }
  if (which == 3){
    pt_printsend("MTC Registers:\n");
    for (i=0;i<21;i++)
      pt_printsend("%2d: (%08x) %s\n",i,mtc_reg_addresses[i],mtc_reg_names[i]);
  }
  if (which == 7 || which == 9){
    pt_printsend("print_connected\t\tPrints all connected devices\n");
    pt_printsend("stop_logging\t\tStop logging output to a file\n");
    pt_printsend("start_logging\t\tStart logging output to a file\n");
    pt_printsend("set_location\t\tSet current location\n");
    pt_printsend("sbc_control\t\tConnect or reconnect to sbc\n");
    pt_printsend("clear_screen\t\tClear screen of output\n");
    pt_printsend("reset_speed\t\tReset speed calculated for pedestal runs\n");
    pt_printsend("kill_threads\t\tClose all currently running threads\n");
    pt_printsend("run_macro\t\tRun a list of commands from a file\n");
    pt_printsend("stop_macro\t\tStop running a macro\n");
  }
  if (which == 4 || which == 9){
    pt_printsend("xr\t\t\tRead from an xl3 register specified by number\n");
    pt_printsend("xw\t\t\tWrite to an xl3 register specified by number\n");
    pt_printsend("xl3_rw\t\t\tRead/Write to xl3/fec registers by address\n");
    pt_printsend("xl3_queue_rw\t\tRead/Write to xl3/fec using command queue\n");
    pt_printsend("debugging_on\t\tTurn on debugging output to serial port\n");
    pt_printsend("debugging_off\t\tTurn off debugging output to serial port\n");
    pt_printsend("change_mode\t\tTurn readout on or off\n");
    pt_printsend("sm_reset\t\tReset vhdl state mahine\n");
    pt_printsend("read_local_voltage\t\tRead a voltage on the xl3\n");
    pt_printsend("hv_readbck\t\tMonitor HV voltage and current\n");
    pt_printsend("crate_init\t\tInitialize crate\n");
  }
  if (which == 5 || which == 9){
    pt_printsend("fr\t\t\tRead from a fec register specified by number\n");
    pt_printsend("fw\t\t\tWrite to a fec register specified by number\n");
    pt_printsend("read_bundle\t\tRead a single bundle and print it\n");
  }
  if (which == 6 || which == 9){
    pt_printsend("mr\t\t\tRead from a mtc register specified by number\n");
    pt_printsend("mw\t\t\tWrite to a mtc register specified by number\n");
    pt_printsend("mtc_read\t\tRead from a mtc register by address\n");
    pt_printsend("mtc_write\t\tWrite to a mtc register by address\n");
    pt_printsend("mtc_init\t\tInitialize mtc\n");
    pt_printsend("set_mtca_thresholds\n");
    pt_printsend("set_gt_mask\n");
    pt_printsend("set_gt_crate_mask\n");
    pt_printsend("set_ped_crate_mask\n");
    pt_printsend("enable_pulser\n");
    pt_printsend("disable_pulser\n");
    pt_printsend("enable_pedestal\n");
    pt_printsend("disable_pedestal\n");
    pt_printsend("set_pulser_freq\n");
    pt_printsend("send_softgt\n");
    pt_printsend("multi_softgt\n");
  }
  if (which == 8 || which == 9){
    pt_printsend("run_pedestals\t\tEnable pedestals on mtc and readout on crates\n");
    pt_printsend("run_pedestals_mtc\t\tEnable pedestals and pulser on mtc\n");
    pt_printsend("run_pedestals_crate\t\tEnable pedestals and readout on crate\n");
    pt_printsend("run_pedestals_end\t\tStop pulser and end readout\n");
    pt_printsend("run_pedestals_end_mtc\t\tStop pulser and pedestals\n");
    pt_printsend("run_pedestals_end_crate\t\tDisable pedestals and stop readout\n");
    pt_printsend("trigger_scan\n");
    pt_printsend("fec_test\n");
    pt_printsend("mem_test\n");
    pt_printsend("vmon\n");
    pt_printsend("board_id\n");
    pt_printsend("ped_run\n");
    pt_printsend("zdisc\n");
    pt_printsend("crate_cbal\n");
    pt_printsend("cgt_test\n");
    pt_printsend("cmos_m_gtvalid\n");
    pt_printsend("cald_test\n");
    pt_printsend("get_ttot\n");
    pt_printsend("set_ttot\n");
    pt_printsend("fifo_test\n");
    pt_printsend("disc_check\n");
    pt_printsend("mb_stability_test\n");
    pt_printsend("chinj_scan\n");
    pt_printsend("final_test\n");
  }
  pt_printsend("-----------------------------------------\n");

  return 0;
}

int read_from_tut(char *result)
{
  reading_from_tut = 1;
  pthread_mutex_lock(&final_test_cmd_lock);
  pthread_cond_wait(&final_test_cmd_cv, &final_test_cmd_lock);
  strcpy(result,final_test_cmd);
  memset(final_test_cmd,'\0',sizeof(final_test_cmd));
  pthread_mutex_unlock(&final_test_cmd_lock);
  reading_from_tut = 0;
  return 0;
}

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
  char temp_command[250];
  strcpy(temp_command,macro_cmds[macro_cur_cmd]);
  int result = process_control_command(temp_command);
  if (result == 0){
    pt_printsend("Finished command %d of %d\n",macro_cur_cmd+1,macro_tot_cmds);
    macro_cur_cmd++;
  }else if (result == 999){
    printf("Problem in macro!\n");
    macro_cur_cmd = macro_tot_cmds;
  }
  return 0; 
}

int parse_macro(char *filename)
{
  FILE *macro_file;
  char long_filename[250];
  sprintf(long_filename,"%s/macro/%s",PENN_DAQ_ROOT,filename);
  macro_file = fopen(long_filename,"r");
  if (macro_file == NULL){
    pt_printsend("Could not open macro file!\n");
    return 0;
  }
  macro_tot_cmds = 0;
  macro_cur_cmd = 0;
  while (fgets(macro_cmds[macro_tot_cmds],250,macro_file) != NULL){
    printf("new command: .%s.\n",macro_cmds[macro_tot_cmds]);
    macro_tot_cmds++;
  }
  fclose(macro_file);
  if (macro_tot_cmds > 0)
    running_macro = 1;
  printf("done reading macro. Going to do %d commands.\n",macro_tot_cmds);
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
    if ((rw_sbc_fd > 0) == 0 && (sbc != 2)){
      if (running_macro == 0)
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
      if (rw_xl3_fd[i] <= 0){
        if (running_macro == 0)
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
    if (running_macro == 0)
      pt_printsend("All threads busy currently\n");
    return -4;
  }

  // we have a thread, so lock everything down
  //pthread_mutex_lock(&socket_lock);
  if (sbc)
    sbc_lock = 1;
  for (i=0;i<19;i++)
    if ((0x1<<i) & crate_mask)
      xl3_lock[i] = 1;
  //pthread_mutex_unlock(&socket_lock);

  return thread_num;
}

int temp_unlock(uint32_t crate_mask)
{
  pthread_mutex_lock(&socket_lock);
  int i;
  for (i=0;i<19;i++)
    if ((0x1<<i) & crate_mask)
      xl3_lock[i] = 2;
  pthread_mutex_unlock(&socket_lock);
}

int relock(uint32_t crate_mask)
{
  pthread_mutex_lock(&socket_lock);
  int i;
  for (i=0;i<19;i++)
    if ((0x1<<i) & crate_mask)
      xl3_lock[i] = 1;
  pthread_mutex_unlock(&socket_lock);
}

int read_configuration_file()
{
  FILE *config_file;
  char filename[1000];
  sprintf(filename,"%s/%s",PENN_DAQ_ROOT,CONFIG_FILE_LOC);
  config_file = fopen(filename,"r");
  if (config_file == NULL){
    printf("Could not open configuration file! Looking for %s\n",filename);
    return -1;
  }
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
        }else if (strcmp(var_name,"DB_VIEWDOC")==0){
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
        }else if (strcmp(var_name,"ORCA_READOUT_PATH")==0){
          strcpy(ORCA_READOUT_PATH,var_value);
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
	  if (strncmp(var_value,"penn",3) == 0)
	    CURRENT_LOCATION = PENN_TESTSTAND;
	  else if (strncmp(var_value,"ag",2) == 0)
	    CURRENT_LOCATION = ABOVE_GROUND_TESTSTAND;
	  else
	    CURRENT_LOCATION = UNDERGROUND;
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
        CURRENT_LOCATION = PENN_TESTSTAND;
      }
      if (words[1] == 'u'){
        printsend("location set to underground\n");
        CURRENT_LOCATION = UNDERGROUND;
      }
      if (words[1] == 'a'){
        printsend("location set to above ground test stand\n");
        CURRENT_LOCATION = ABOVE_GROUND_TESTSTAND;
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
    char log_name[500] = {'\0'};  // random size, it's a pretty nice number though.
    time_t curtime = time(NULL);
    struct timeval moretime;
    gettimeofday(&moretime,0);
    struct tm *loctime = localtime(&curtime);

    sprintf(log_name,"%s/log/",PENN_DAQ_ROOT);
    strftime(log_name+strlen(log_name), 256, "%Y_%m_%d_%H_%M_%S_", loctime);
    sprintf(log_name+strlen(log_name), "%d.log", (int)moretime.tv_usec);
    ps_log_file = fopen(log_name, "a+");
    if (ps_log_file == NULL){
	printsend("Problem enabling logging: Could not open log file!\n");
	write_log = 0;
    }else{
	printsend( "Enabled logging\n");
	printsend( "Opened log file: %s\n", log_name);
    }

  }
  else{
    printsend("Logging already enabled\n");
  }
  return 0;
}

int start_logging_to_file(char * filename){
  if (write_log){
    stop_logging();
  }
  write_log = 1;
  char log_name[500] = {'\0'};  // random size, it's a pretty nice number though.

  sprintf(log_name,"%s/log/%s",PENN_DAQ_ROOT,filename);
  ps_log_file = fopen(filename, "a+");
  if (ps_log_file == NULL){
    printsend("Problem enabling logging: Could not open log file!\n");
    write_log = 0;
  }else{
    printsend( "Enabled logging\n");
    printsend( "Opened log file: %s\n", log_name);
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

uint32_t sGetBits(uint32_t value, uint32_t bit_start, uint32_t num_bits)
{
  uint32_t bits;
  bits = (value >> (bit_start + 1 - num_bits)) & ~(~0 << num_bits);
  return bits;
}
