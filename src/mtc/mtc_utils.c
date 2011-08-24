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

#include "main.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "mtc_utils.h"

int sbc_control(char *buffer)
{
  sbc_control_t *args;
  args = malloc(sizeof(sbc_control_t));

  args->sbc_action = 0;
  strcpy(args->identity_file,"");

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
      }else if (words[1] == 'h'){
        printsend("Usage: sbc_control -c (connect)|-k (kill)|-r (reconnect)"
          "-i [identity file]\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }
  
  // now check and see if everything needed is unlocked
  if (sbc_lock){
    // this xl3 is locked, we cant do this right now
    free(args);
    return -1;
  }
  // spawn a thread to do it
  sbc_lock = 1;

  pthread_t *new_thread;
  new_thread = malloc(sizeof(pthread_t));
  int i,thread_num = -1;
  for (i=0;i<MAX_THREADS;i++){
    if (thread_pool[i] == NULL){
      thread_pool[i] = new_thread;
      thread_num = i;
      break;
    }
  }
  if (thread_num == -1){
    printsend("All threads busy currently\n");
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
    pt_printsend("sbc_control: Connecting to the SBC\n");
    if (rw_sbc_fd > 0){
      pt_printsend("sbc_control: Already was connected (socket %d)\n",rw_sbc_fd);
    }else{
      char start_cmd[500];
      sprintf(start_cmd,"%s /etc/rc.d/orcareadout start",base_cmd);
      pt_printsend("sbc_control: Starting remote OrcaReadout process\n");
      system(start_cmd);
      rw_sbc_fd = socket(AF_INET, SOCK_STREAM, 0);
      if (rw_sbc_fd <= 0){
        pt_printsend("sbc_control: Error opening a new socket\n");
      }else{
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
        }else{
          FD_SET(rw_sbc_fd,&main_fdset);
          if (rw_sbc_fd > fdmax)
            fdmax = rw_sbc_fd;
          // now send test word to OrcaReadout so it can
          // determine endianness
          int32_t test_word = 0x000DCBA;
          int n = write(rw_sbc_fd,(char*)&test_word,4);
          pt_printsend("sbc_connect: Connected to SBC\n");
          sbc_connected = 1;
        } // end if connected correctly
      } // end if socket opened correctly
    } // end if wasnt already connected
  } // end if connecting or reconnecting
  pthread_mutex_unlock(&main_fdset_lock);

  sbc_lock = 0;
  thread_done[thread_num] = 1;
}
