#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "net_utils.h"
#include "xl3_utils.h"

int debugging_mode(char *buffer, int onoff)
{
  char *words,*words2;
  int crate_num = 0;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          crate_num = atoi(words2);
      }else if (words[1] == 'h'){
        printsend("Usage: debugging_on/off -c [crate num (int)]\n");
        return 1;
      }
    }
    words = strtok(NULL, " ");
  }
  
  // now check and see if everything needed is unlocked
  if (xl3_lock[crate_num] == 0){
    // spawn a thread to do it
    xl3_lock[crate_num] = 1;

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
      return -1;
    }
    uint32_t *args;
    args = malloc(3*sizeof(uint32_t));
    args[0] = (uint32_t) thread_num;
    args[1] = (uint32_t) crate_num;
    args[2] = (uint32_t) onoff;
    pthread_create(new_thread,NULL,pt_debugging_mode,(void *)args);
    return 1; 
  }else{
    // this xl3 is locked, we cant do this right now
    return -1;
  }
}

void *pt_debugging_mode(void* args)
{
  int thread_num = ((uint32_t *) args)[0];
  int crate_num = ((uint32_t *) args)[1];
  int onoff = ((uint32_t *) args)[2];

  printf("hello\n");
  xl3_lock[crate_num] = 0;
  thread_done[thread_num] = 1;
}
