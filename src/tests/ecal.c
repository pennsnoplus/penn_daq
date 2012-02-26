#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "packet_types.h"
#include "db_types.h"
#include "pouch.h"
#include "json.h"

#include "db.h"
#include "xl3_rw.h"
#include "net.h"
#include "net_utils.h"
#include "daq_utils.h"
#include "ecal.h"

int ecal(char *buffer)
{
  ecal_t *args;
  args = malloc(sizeof(ecal_t));

  args->crate_mask = 0x4;
  int i;
  for (i=0;i<19;i++)
    args->slot_mask[i] = 0xFFFF;
  args->update_hwdb = 0;
  args->old_ecal = 0;
  args->noise_run = 0;
 
  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          for (i=0;i<19;i++)
            args->slot_mask[i] = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == '0'){
        if (words[2] == '0'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[0] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[1] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '2'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[2] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '3'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[3] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '4'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[4] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '5'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[5] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '6'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[6] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '7'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[7] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '8'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[8] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '9'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[9] = strtoul(words2,(char**)NULL,16);
        }
      }else if (words[1] == '1'){   
        if (words[2] == '0'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[10] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '1'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[11] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '2'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[12] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '3'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[13] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '4'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[14] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '5'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[15] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '6'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[16] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '7'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[17] = strtoul(words2,(char**)NULL,16);
        }else if (words[2] == '8'){
          if ((words2 = strtok(NULL," ")) != NULL)
            args->slot_mask[18] = strtoul(words2,(char**)NULL,16);
        }

      }else if (words[1] == 'd'){
        args->update_hwdb = 1;
      }else if (words[1] == 'n'){
        args->noise_run = 1;
      }else if (words[1] == '#'){
        if ((words2 = strtok(NULL," ")) != NULL){
          args->old_ecal = 1;
          strcpy(args->ecal_id,words2);
        }
      }else if (words[1] == 'h'){
        pt_printsend("Usage: ecal -c [crate mask (hex)] "
            "-s [slot mask for all crates (hex)] "
            "-00...-18 [slot mask for that crate (hex)] "
            "-d (update FEC database) "
            "-n (do noise run) "
            "-# [previously run ecal id (hex) to update db with those values]\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(1,args->crate_mask,&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_ecal,(void *)args);
  return 0; 
}


void *pt_ecal(void *args)
{
  ecal_t arg = *(ecal_t *) args; 
  free(args);

  int i,j;
  int result;

  char ecal_id[250];
  // now we can unlock our things, since nothing else should use them

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  for (i=0;i<19;i++){
    if ((0x1<<i) & arg.crate_mask)
      FD_SET(rw_xl3_fd[i],&thread_fdset);
  }

  char comments[1000];
  memset(comments,'\0',1000);
  char command_buffer[1000];
  memset(command_buffer,'\0',1000);

  system("clear");
  pt_printsend("------------------------------------------\n");
  pt_printsend("Welcome to ECAL+!\n");
  pt_printsend("------------------------------------------\n");



  if (!arg.old_ecal){
    // once this is set we can no longer send other commands
    running_ecal = 1;
    sbc_lock = 0;
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask)
        xl3_lock[i] = 0;
    }



    get_new_id(ecal_id);


    pt_printsend("\nYou have selected the following slots:\n\n");
    for (i=0;i<19;i++){
      if ((0x1<<(i)) & arg.crate_mask){
        pt_printsend("crate %d: 0x%08x\n",i,arg.slot_mask[i]);
      }
    }
    pt_printsend("------------------------------------------\n");
    pt_printsend("Hit enter to start, or type quit if anything is incorrect\n");
    read_from_tut(comments);
    if (strncmp("quit",comments,4) == 0){
      pt_printsend("Exiting ECAL\n");
      running_ecal = 0;
      unthread_and_unlock(0,0x0,arg.thread_num); 
      return;
    }
    pt_printsend("------------------------------------------\n");

    time_t curtime = time(NULL);
    struct timeval moretime;
    gettimeofday(&moretime,0);
    struct tm *loctime = localtime(&curtime);
    char log_name[500] = {'\0'};  // random size, it's a pretty nice number though.

    strftime(log_name, 256, "ECAL_%Y_%m_%d_%H_%M_%S_", loctime);
    sprintf(log_name+strlen(log_name), "%d.log", (int)moretime.tv_usec);
    start_logging_to_file(log_name);

    sbc_lock = 1;
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask)
        xl3_lock[i] = 1;
    }



    pt_printsend("Creating ECAL document...\n");
    // post ecal doc
    post_ecal_doc(arg.crate_mask,arg.slot_mask,log_name,ecal_id,&thread_fdset);

    pt_printsend("Created! ECAL id: %s\n\n",ecal_id);
    pt_printsend("------------------------------------------\n");
    
    sbc_lock = 0;
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask)
        xl3_lock[i] = 0;
    }

    // ok we are set up, time to start

    // initial CRATE_INIT
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -x -v",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // FEC_TEST
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"fec_test -c %d -s %04x -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = fec_test(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }

        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("-------------------------------------------\n");
      }
    }


    // BOARD_ID
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"board_id -c %d -s %04x",i,arg.slot_mask[i]);
          result = board_id(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }

        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("-------------------------------------------\n");
      }
    }


    // MTC_INIT
    do {
      sprintf(command_buffer,"mtc_init -x");
      result = mtc_init(command_buffer);
      if (result == -2 || result == -3){
        running_ecal = 0;
        unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");

    

    // CGT_TEST

    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"cgt_test_1 -c %d -s %04x -p FFFFFFFF -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = cgt_test(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }

        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("-------------------------------------------\n");
      }
    }

    // MTC_INIT
    do {
      sprintf(command_buffer,"mtc_init -x");
      result = mtc_init(command_buffer);
      if (result == -2 || result == -3){
        running_ecal = 0;
        unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // CRATE_CBAL
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_cbal -c %d -s %04x -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = crate_cbal(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // CRATE_INIT with vbal values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("-------------------------------------------\n");
      }
    }


    // PED_RUN
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"ped_run -c %d -s %04x -b -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = ped_run(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("-------------------------------------------\n");
      }
    }

//    pt_printsend("Time to do the cable business\n");
//    pt_printsend("ECL output --> EXT PED (long cable)\n");
//    pt_printsend("TTL input --> Global trigger)\n");
//    pt_printsend("Hit enter when ready\n");

//    read_from_tut(comments);

    // MTC_INIT
    do {
      sprintf(command_buffer,"mtc_init -x");
      result = mtc_init(command_buffer);
      if (result == -2 || result == -3){
        running_ecal = 0;
        unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default + vbal values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // SET_TTOT
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"set_ttot -c %d -s %04x -t 420 -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = set_ttot(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // CRATE_INIT with default + vbal + tdisc values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // GET_TTOT
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"get_ttot -c %d -s %04x -t 400 -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = get_ttot(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


//    pt_printsend("Time to remove cables\n");
//    pt_printsend("Hit enter when ready\n");

//    read_from_tut(comments);


    // CRATE_INIT with default + vbal + tdisc values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // DISC_CHECK
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"disc_check -c %d -s %04x -n 500000 -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = disc_check(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // MTC_INIT
    do {
      sprintf(command_buffer,"mtc_init -x");
      result = mtc_init(command_buffer);
      if (result == -2 || result == -3){
        running_ecal = 0;
        unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default + vbal + tdisc values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // CMOS_M_GTVALID
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"cmos_m_gtvalid -c %d -s %04x -g 410 -n -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = cmos_m_gtvalid(command_buffer); 
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // MTC_INIT
    do {
      sprintf(command_buffer,"mtc_init -x");
      result = mtc_init(command_buffer);
      if (result == -2 || result == -3){
        running_ecal = 0;
        unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default + vbal + tdisc + tcmos values
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D -C",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // ZDISC
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"zdisc -c %d -s %04x -o 0 -r 10000 -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = zdisc(command_buffer);
          if (result == -2 || result == -3){
            running_ecal = 0;
            unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }

    pt_printsend("-------------------------------------------\n");
    pt_printsend("ECAL finished.\n");

    sbc_lock = 1;
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask)
        xl3_lock[i] = 1;
    }



  }else{
    sprintf(ecal_id,"%s",arg.ecal_id);
  }


  if (arg.update_hwdb){
    pt_printsend("Now updating FEC database with test results\n");

    // get the ecal document with the configuration
    char get_db_address[500];
    sprintf(get_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,ecal_id);
    pouch_request *ecaldoc_response = pr_init();
    pr_set_method(ecaldoc_response, GET);
    pr_set_url(ecaldoc_response, get_db_address);
    pr_do(ecaldoc_response);
    if (ecaldoc_response->httpresponse != 200){
      pt_printsend("Unable to connect to database. error code %d\n",(int)ecaldoc_response->httpresponse);
      running_ecal = 0;
      unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
      return;
    }
    JsonNode *ecalconfig_doc = json_decode(ecaldoc_response->resp.data);

    // get all the ecal test results for all crates/slots
    sprintf(get_db_address,"%s/%s/%s/get_ecal?startkey=\"%s\"&endkey=\"%s\"",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,ecal_id,ecal_id);
    pouch_request *ecal_response = pr_init();
    pr_set_method(ecal_response, GET);
    pr_set_url(ecal_response, get_db_address);
    pr_do(ecal_response);
    if (ecal_response->httpresponse != 200){
      pt_printsend("Unable to connect to database. error code %d\n",(int)ecal_response->httpresponse);
      running_ecal = 0;
      unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
      return;
    }
    JsonNode *ecalfull_doc = json_decode(ecal_response->resp.data);
    JsonNode *ecal_rows = json_find_member(ecalfull_doc,"rows");
    int total_rows = json_get_num_mems(ecal_rows); 
    if (total_rows == 0){
      pt_printsend("No documents for this ECAL yet! (id %s)\n",ecal_id);
      running_ecal = 0;
      unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
      return;
    }

    // loop over crates/slots, create a fec document for each
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        for (j=0;j<16;j++){
          if ((0x1<<j) & arg.slot_mask[i]){
            printf("crate %d slot %d\n",i,j);

            // lets generate the fec document
            JsonNode *doc;
            create_fec_db_doc(i,j,&doc,ecalconfig_doc);

            int k;
            for (k=0;k<total_rows;k++){
              JsonNode *ecalone_row = json_find_element(ecal_rows,k);
              JsonNode *test_doc = json_find_member(ecalone_row,"value");
              JsonNode *config = json_find_member(test_doc,"config");
              if ((json_get_number(json_find_member(config,"crate_id")) == i) && (json_get_number(json_find_member(config,"slot")) == j)){
                if (strcmp(json_get_string(json_find_member(test_doc,"type")),"find_noise") != 0){
                  printf("test type is %s\n",json_get_string(json_find_member(test_doc,"type")));
                  add_ecal_test_results(doc,test_doc);
                }
              }
            }

            post_fec_db_doc(i,j,doc);

            json_delete(doc); // only delete the head node
          }
        }
      }
    }

    json_delete(ecalfull_doc);
    pr_free(ecal_response);
    json_delete(ecalconfig_doc);
    pr_free(ecaldoc_response);
  }

  if (arg.noise_run){
    // re lock everything down
    sbc_lock = 0;
    for (i=0;i<19;i++)
      if ((0x1<<i) & arg.crate_mask)
        xl3_lock[i] = 0;

    // FIND_NOISE
    int some_crate = 0;
    do{
      sprintf(command_buffer,"find_noise -c %05x -d -E %s ",arg.crate_mask,ecal_id);
      for (i=0;i<19;i++){
        if ((0x1<<i) & arg.crate_mask){
          some_crate = i;
        }
        sprintf(command_buffer+strlen(command_buffer),"-%02d %04x ",i,arg.slot_mask[i]);
      }
      result = find_noise(command_buffer);
      if (result == -2 || result == -3){
        printf("result was %d\n",result);
        running_ecal = 0;
        unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
        return;
      }
    } while (result != 0);
    while (xl3_lock[some_crate] != 0){}
    pt_printsend("------------------------------------------\n");
  }



  // now update again with noise run stuff
  if (arg.update_hwdb){

    // re lock everything down
    sbc_lock = 0;
    for (i=0;i<19;i++)
      if ((0x1<<i) & arg.crate_mask)
        xl3_lock[i] = 0;

    pt_printsend("Updating hw db with find_noise results\n");

    // get the find noise test results
    char get_db_address[500];
    sprintf(get_db_address,"%s/%s/%s/get_ecal?startkey=\"%s\"&endkey=\"%s\"",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,ecal_id,ecal_id);
    pouch_request *ecal_response = pr_init();
    pr_set_method(ecal_response, GET);
    pr_set_url(ecal_response, get_db_address);
    pr_do(ecal_response);
    if (ecal_response->httpresponse != 200){
      pt_printsend("Unable to connect to database. error code %d\n",(int)ecal_response->httpresponse);
      running_ecal = 0;
      unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
      return;
    }
    JsonNode *ecalfull_doc = json_decode(ecal_response->resp.data);
    JsonNode *ecal_rows = json_find_member(ecalfull_doc,"rows");
    int total_rows = json_get_num_mems(ecal_rows); 
    if (total_rows == 0){
      pt_printsend("No documents for this ECAL yet! (id %s)\n",ecal_id);
      running_ecal = 0;
      unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
      return;
    }

    // loop over crates/slots
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        for (j=0;j<16;j++){
          if ((0x1<<j) & arg.slot_mask[i]){
printf("crate %d slot %d\n",i,j);
            // get the current fec document
            sprintf(get_db_address,"%s/%s/%s/get_fec?startkey=[%d,%d,\"\"]&endkey=[%d,%d]&descending=true",FECDB_SERVER,FECDB_BASE_NAME,FECDB_VIEWDOC,i,j+1,i,j);
            pouch_request *fec_response = pr_init();
            pr_set_method(fec_response, GET);
            pr_set_url(fec_response, get_db_address);
            pr_do(fec_response);
            if (fec_response->httpresponse != 200){
              pt_printsend("Unable to connect to database. error code %d\n",(int)fec_response->httpresponse);
              unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
              return;
            }
            JsonNode *fecfull_doc = json_decode(fec_response->resp.data);
            JsonNode *fec_rows = json_find_member(fecfull_doc,"rows");
            int total_rows2 = json_get_num_mems(fec_rows); 
            if (total_rows2 == 0){
              pt_printsend("No FEC documents for this crate/card yet! (crate %d card %d)\n",i,j);
              unthread_and_unlock(1,arg.crate_mask,arg.thread_num);
              return;
            }
            JsonNode *fecone_row = json_find_element(fec_rows,0);
            JsonNode *fec_doc = json_find_member(fecone_row,"value");

            // now find the noise run document for this crate/slot and add it to the fec document
            int found_it = 0;
            int k;
            for (k=0;k<total_rows;k++){
              JsonNode *ecalone_row = json_find_element(ecal_rows,k);
              JsonNode *test_doc = json_find_member(ecalone_row,"value");
              JsonNode *config = json_find_member(test_doc,"config");
              if ((json_get_number(json_find_member(config,"crate_id")) == i) && (json_get_number(json_find_member(config,"slot")) == j)){
                if (strcmp(json_get_string(json_find_member(test_doc,"type")),"find_noise") == 0){
                  found_it = 1;
                  printf("test type is %s\n",json_get_string(json_find_member(test_doc,"type")));
                  add_ecal_test_results(fec_doc,test_doc);
                  break;
                }
              }
            }
            if (found_it == 0){
              pt_printsend("Couldn't find noise run results!\n");
            }else{
              // push the updated fec document
              update_fec_db_doc(fec_doc);
            }
            json_delete(fecfull_doc);
            pr_free(fec_response);
          }
        }
      }
    }

    json_delete(ecalfull_doc);
    pr_free(ecal_response);
  }


  running_ecal = 0;
  unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
}

