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

int sbc_control(char *buffer)
{
  sbc_control_t *args;
  args = malloc(sizeof(sbc_control_t));

  args->sbc_action = 0;
  args->manual = 0;
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

int mtc_xilinx_load()
{
  char *data;
  long howManybits;
  long bitIter;
  uint32_t word;
  uint32_t dp;
  uint32_t temp;

  pt_printsend("loading xilinx\n");
  data = getXilinxData(&howManybits);
  pt_printsend("Got %d bits of xilinx data\n",howManybits);
  if ((data == NULL) || (howManybits == 0)){
    pt_printsend("error getting xilinx data\n");
    return -1;
  }

  SBC_Packet packet; 

  packet.cmdHeader.destination = 0x3;
  packet.cmdHeader.cmdID = 0x1;
  packet.cmdHeader.numberBytesinPayload = sizeof(SNOMtc_XilinxLoadStruct) + howManybits;
  packet.numBytes = packet.cmdHeader.numberBytesinPayload+256+16;
  SNOMtc_XilinxLoadStruct *payloadPtr = (SNOMtc_XilinxLoadStruct *)packet.payload;
  payloadPtr->baseAddress = 0x7000;
  payloadPtr->addressModifier = 0x29;
  payloadPtr->errorCode = 0;
  payloadPtr->programRegOffset = MTCXilProgReg;
  payloadPtr->fileSize = howManybits;
  char *p = (char *)payloadPtr + sizeof(SNOMtc_XilinxLoadStruct);
  strncpy(p, data, howManybits);
  
  printf("sending do_cmd a packet of size %d\n",sizeof(packet));

  do_mtc_xilinx_cmd(packet);
  long errorCode = payloadPtr->errorCode;
  if (errorCode){
    pt_printsend( "Error code: %d \n",(int)errorCode);
  }
  pt_printsend("Xilinx loading complete\n");

  free(data);
  data = (char *) NULL;
  return 0;
}

static char* getXilinxData(long *howManyBits)
{
  char c;
  FILE *fp;
  char *data = NULL;

  if ((fp = fopen(MTC_XILINX_LOCATION, "r")) == NULL ) {
    pt_printsend( "getXilinxData:  cannot open file %s\n", MTC_XILINX_LOCATION);
    return (char*) NULL;
  }

  if ((data = (char *) malloc(MAX_DATA_SIZE)) == NULL) {
    //perror("GetXilinxData: ");
    pt_printsend("GetXilinxData: malloc error\n");
    return (char*) NULL;
  }

  /* skip header -- delimited by two slashes. 
     if ( (c = getc(fp)) != '/') {
     fprintf(stderr, "Invalid file format Xilinx file.\n");
     return (char*) NULL;
     }
     while (( (c = getc(fp))  != EOF ) && ( c != '/'))
     ;
     */
  /* get real data now. */
  *howManyBits = 0;
  while (( (data[*howManyBits] = getc(fp)) != EOF)
      && ( *howManyBits < MAX_DATA_SIZE)) {
    /* skip newlines, tabs, carriage returns */
    if ((data[*howManyBits] != '\n') &&
        (data[*howManyBits] != '\r') &&
        (data[*howManyBits] != '\t') ) {
      (*howManyBits)++;
    }


  }
  fclose(fp);
  return data;
}

