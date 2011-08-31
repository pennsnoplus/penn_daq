#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "packet_types.h"
#include "mtc_registers.h"

#include "main.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_rw.h"
#include "mtc_utils.h"
#include "mtc_cmds.h"

int sbc_control(char *buffer)
{
  sbc_control_t *args;
  args = malloc(sizeof(sbc_control_t));

  args->sbc_action = 1;
  args->manual = 0;
  strcpy(args->identity_file,DEFAULT_SSHKEY);

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        args->sbc_action = 0;
      }else if (words[1] == 'r'){
        args->sbc_action = 1;
      }else if (words[1] == 'k'){
        args->sbc_action = 2;
      }else if (words[1] == 'i'){
        words2 = strtok(NULL, " ");
        strcpy(args->identity_file,words2);
      }else if (words[1] == 'm'){
        args->manual = 1;
      }else if (words[1] == 'h'){
        printsend("Usage: sbc_control -c (connect)|-k (kill)|-r (reconnect)"
            "-i [identity file] -m (start OrcaReadout manually)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(2,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;

  pthread_create(new_thread,NULL,pt_sbc_control,(void *)args);
  return 0; 
}

void *pt_sbc_control(void *args)
{
  int thread_num = ((sbc_control_t *) args)->thread_num;
  int sbc_action = ((sbc_control_t *) args)->sbc_action;
  int manual = ((sbc_control_t *) args)->manual;
  char identity_file[100];
  strcpy(identity_file,((sbc_control_t *) args)->identity_file);
  free(args);

  char base_cmd[100];
  if (strcmp(identity_file,"") != 0)
    sprintf(base_cmd,"ssh %s@%s -i %s",SBC_USER,SBC_SERVER,identity_file);
  else
    sprintf(base_cmd,"ssh %s@%s",SBC_USER,SBC_SERVER);

  pthread_mutex_lock(&main_fdset_lock);
  // if killing or reconnecting, close socket and stop the service
  if (sbc_action == 1 || sbc_action == 2){
    if (rw_sbc_fd > 0){
      close(rw_sbc_fd);
      FD_CLR(rw_sbc_fd,&main_fdset);
      rw_sbc_fd = -1;
      sbc_connected = 0;
    }
    char kill_cmd[500];
    sprintf(kill_cmd,"%s /etc/rc.d/orcareadout stop",base_cmd);
    pt_printsend("sbc_control: Stopping remote OrcaReadout process\n");
    system(kill_cmd);
  }

  // if connecting or reconnecting, do that now
  if (sbc_action == 0 || sbc_action == 1)
  {
    printf("connecting or reconnecting\n");
    pt_printsend("sbc_control: Connecting to the SBC\n");
    if (rw_sbc_fd > 0){
      pt_printsend("sbc_control: Already was connected (socket %d)\n",rw_sbc_fd);
      pthread_mutex_unlock(&main_fdset_lock);
      sbc_lock = 0;
      thread_done[thread_num] = 1;
      return;
    }

    if (manual == 0){
      char start_cmd[500];
      sprintf(start_cmd,"%s /etc/rc.d/orcareadout start",base_cmd);
      pt_printsend("sbc_control: Starting remote OrcaReadout process\n");
      system(start_cmd);
    }

    rw_sbc_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (rw_sbc_fd <= 0){
      pt_printsend("sbc_control: Error opening a new socket\n");
      pthread_mutex_unlock(&main_fdset_lock);
      sbc_lock = 0;
      thread_done[thread_num] = 1;
      return;
    }

    usleep(1000);

    // we can connect, set up the address
    struct sockaddr_in sbc_addr;
    memset(&sbc_addr,'\0',sizeof(sbc_addr));
    sbc_addr.sin_family = AF_INET;
    inet_pton(AF_INET, SBC_SERVER, &sbc_addr.sin_addr.s_addr);
    sbc_addr.sin_port = htons(SBC_PORT);
    // make the connection
    if (connect(rw_sbc_fd,(struct sockaddr*) &sbc_addr,sizeof(sbc_addr))<0){
      close(rw_sbc_fd);
      FD_CLR(rw_sbc_fd,&main_fdset);
      rw_sbc_fd = -1;
      pt_printsend("sbc_control: Error connecting to SBC (errno %d)\n",errno);
      pthread_mutex_unlock(&main_fdset_lock);
      sbc_lock = 0;
      thread_done[thread_num] = 1;
      return;
    }

    FD_SET(rw_sbc_fd,&main_fdset);
    if (rw_sbc_fd > fdmax)
      fdmax = rw_sbc_fd;
    // now send test word to OrcaReadout so it can
    // determine endianness
    int32_t test_word = 0x000DCBA;
    int n = write(rw_sbc_fd,(char*)&test_word,4);
    pt_printsend("sbc_connect: Connected to SBC\n");
    sbc_connected = 1;
  } // end if connecting or reconnecting

  pthread_mutex_unlock(&main_fdset_lock);
  sbc_lock = 0;
  thread_done[thread_num] = 1;
}

int mtc_read(char *buffer)
{
  mtc_read_t *args;
  args = malloc(sizeof(mtc_read_t));

  args->address = 0x0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'a'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->address = strtoul(words2,(char **) NULL,16);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: mtc_read -a [address (hex)]\n");
        pt_printsend("Please check mtc/mtc_registers.h for the address mapping\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  // we have a thread so lock it
  sbc_lock = 1;

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_mtc_read,(void *)args);
  return 0; 
}

void *pt_mtc_read(void *args)
{
  mtc_read_t arg = *(mtc_read_t *) args;
  free(args);

  uint32_t data = 0x0;
  mtc_reg_read(arg.address, &data);
  pt_printsend("Received %08x\n",data);

  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int mtc_write(char *buffer)
{
  mtc_write_t *args;
  args = malloc(sizeof(mtc_write_t));

  args->address = 0x0;
  args->data = 0x0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'a'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->address = strtoul(words2,(char **) NULL,16);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->data = strtoul(words2,(char **) NULL,16);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: mtc_write "
            "-a [address (hex)] -d [data (hex)]\n");
        pt_printsend("Please check mtc/mtc_registers.h for the address mapping\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_mtc_write,(void *)args);
  return 0; 
}

void *pt_mtc_write(void *args)
{
  mtc_write_t arg = *(mtc_write_t *) args;
  free(args);

  mtc_reg_write(arg.address, arg.data);
  pt_printsend("Wrote %08x\n",arg.data);

  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int set_mtca_thresholds(char *buffer)
{
  set_mtca_thresholds_t *args;
  args = malloc(sizeof(set_mtca_thresholds_t));

  int i;
  for (i=0;i<14;i++){
    args->dac_voltages[i] = -4.900; 
  }

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == '0'){
        if (words[2] == '0'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[0] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[1] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '2'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[2] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '3'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[3] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '4'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[4] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '5'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[5] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '6'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[6] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '7'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[7] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '8'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[8] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '9'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[9] = (float) strtod(words2,(char**)NULL)*1000;
        }
      }else if (words[1] == '1'){
        if (words[2] == '0'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[10] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[11] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '2'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[12] = (float) strtod(words2,(char**)NULL)*1000;
        }else if (words[2] == '3'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->dac_voltages[13] = (float) strtod(words2,(char**)NULL)*1000;
        }
      }else if (words[1] == 'h'){
        printsend("Usage: set_thresholds -(00)..(13) [voltage level in V (float)]\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  for (i=0;i<14;i++)
    args->dac_voltages[i] *= 1000;

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_set_mtca_thresholds,(void *)args);
  return 0; 
}

void *pt_set_mtca_thresholds(void *args)
{
  set_mtca_thresholds_t arg = *(set_mtca_thresholds_t *) args;
  free (args);

  load_mtca_dacs(arg.dac_voltages);

  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int cmd_set_gt_mask(char *buffer)
{
  set_mask_t *args;
  args = malloc(sizeof(set_mask_t));

  args->mask = 0x0;
  args->ored = 0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'm'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->mask = strtoul(words2,(char **) NULL,16);
      }else if (words[1] == 'o'){
          args->ored = 1;
      }else if (words[1] == 'h'){
        pt_printsend("Usage: set_gt_mask "
            "-m [mask (hex)] -o (\"or\" it with currently set mask)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_set_gt_mask,(void *)args);
  return 0; 
}

void *pt_cmd_set_gt_mask(void *args)
{
  set_mask_t arg = *(set_mask_t *) args;
  free (args);

  uint32_t temp = 0x0;
  if (arg.ored)
    mtc_reg_read(MTCMaskReg, &temp);
  mtc_reg_write(MTCMaskReg, temp & arg.mask);

  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int cmd_set_gt_crate_mask(char *buffer)
{
  set_mask_t *args;
  args = malloc(sizeof(set_mask_t));

  args->mask = 0x0;
  args->ored = 0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'm'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->mask = strtoul(words2,(char **) NULL,16);
      }else if (words[1] == 'o'){
          args->ored = 1;
      }else if (words[1] == 'h'){
        pt_printsend("Usage: set_gt_crate_mask "
            "-m [mask (hex)] -o (\"or\" it with currently set mask)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_set_gt_crate_mask,(void *)args);
  return 0; 
}

void *pt_cmd_set_gt_crate_mask(void *args)
{
  set_mask_t arg = *(set_mask_t *) args;
  free (args);

  uint32_t temp = 0x0;
  if (arg.ored)
    mtc_reg_read(MTCGmskReg, &temp);
  mtc_reg_write(MTCGmskReg, temp & arg.mask);

  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int cmd_set_ped_crate_mask(char *buffer)
{
  set_mask_t *args;
  args = malloc(sizeof(set_mask_t));

  args->mask = 0x0;
  args->ored = 0;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'm'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          args->mask = strtoul(words2,(char **) NULL,16);
      }else if (words[1] == 'o'){
          args->ored = 1;
      }else if (words[1] == 'h'){
        pt_printsend("Usage: set_ped_crate_mask "
            "-m [mask (hex)] -o (\"or\" it with currently set mask)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_set_ped_crate_mask,(void *)args);
  return 0; 
}

void *pt_cmd_set_ped_crate_mask(void *args)
{
  set_mask_t arg = *(set_mask_t *) args;
  free (args);

  uint32_t temp = 0x0;
  if (arg.ored)
    mtc_reg_read(MTCPmskReg, &temp);
  mtc_reg_write(MTCPmskReg, temp & arg.mask);

  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}

int cmd_enable_pulser(char *buffer)
{
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'h'){
        pt_printsend("Usage: enable_pulser\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    return -1;
  }

  int *args = malloc(sizeof(int));
  *args = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_enable_pulser,(void *)args);
  return 0; 
}

void *pt_cmd_enable_pulser(void *args)
{
  int thread_num = *(int *) args;
  free(args);
  enable_pulser();
  sbc_lock = 0;
  thread_done[thread_num] = 1;
  return;
}

int cmd_disable_pulser(char *buffer)
{
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'h'){
        pt_printsend("Usage: disable_pulser\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    return -1;
  }

  int *args = malloc(sizeof(int));
  *args = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_disable_pulser,(void *)args);
  return 0; 
}

void *pt_cmd_disable_pulser(void *args)
{
  int thread_num = *(int *) args;
  free(args);
  disable_pulser();
  sbc_lock = 0;
  thread_done[thread_num] = 1;
  return;
}

int cmd_enable_pedestal(char *buffer)
{
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'h'){
        pt_printsend("Usage: enable_pedestal\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    return -1;
  }

  int *args = malloc(sizeof(int));
  *args = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_enable_pedestal,(void *)args);
  return 0; 
}

void *pt_cmd_enable_pedestal(void *args)
{
  int thread_num = *(int *) args;
  free(args);
  enable_pedestal();
  sbc_lock = 0;
  thread_done[thread_num] = 1;
  return;
}

int cmd_disable_pedestal(char *buffer)
{
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'h'){
        pt_printsend("Usage: disable_pedestal\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    return -1;
  }

  int *args = malloc(sizeof(int));
  *args = thread_num;
  pthread_create(new_thread,NULL,pt_cmd_disable_pedestal,(void *)args);
  return 0; 
}

void *pt_cmd_disable_pedestal(void *args)
{
  int thread_num = *(int *) args;
  free(args);
  disable_pedestal();
  sbc_lock = 0;
  thread_done[thread_num] = 1;
  return;
}

int set_pulser_freq(char *buffer)
{
  set_pulser_freq_t *args;
  args = malloc(sizeof(set_pulser_freq_t));

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'f'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->frequency = atof(words2);
      }else if (words[1] == 'h'){
        pt_printsend("Usage: set_pulser_freq -f [frequency]\n");
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,0x0,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_set_pulser_freq,(void *)args);
  return 0; 
}

void *pt_set_pulser_freq(void *args)
{
  set_pulser_freq_t arg = *(set_pulser_freq_t *) args;
  free(args);

  set_pulser_frequency(arg.frequency);
  sbc_lock = 0;
  thread_done[arg.thread_num] = 1;
  return;
}


