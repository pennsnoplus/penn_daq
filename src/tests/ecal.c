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

  args->crate_mask = 2;
  int i;
  for (i=0;i<19;i++)
    args->slot_mask[i] = 0xFFFF;

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
        }

      }else if (words[1] == 'd'){
        args->update_hwdb = 1;
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
  sbc_lock = 0;
  for (i=0;i<19;i++){
    if ((0x1<<i) & arg.crate_mask)
      xl3_lock[i] = 0;
  }

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  for (i=0;i<19;i++){
    if ((0x1<<i) & arg.crate_mask)
      FD_SET(rw_xl3_fd[i],&thread_fdset);
  }



  if (!arg.old_ecal){
    // once this is set we can no longer send other commands
    running_ecal = 1;


    get_new_id(ecal_id);

    char comments[1000];
    memset(comments,'\0',1000);
    char command_buffer[100];
    memset(command_buffer,'\0',100);

    system("clear");
    pt_printsend("------------------------------------------\n");
    pt_printsend("Welcome to ECAL+!\n");
    pt_printsend("------------------------------------------\n");
    pt_printsend("ECAL id: %s\n\n",ecal_id);
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



    // post ecal doc
    post_ecal_doc(arg.crate_mask,arg.slot_mask,log_name,ecal_id);


    // ok we are set up, time to start

    // initial CRATE_INIT
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -x -v",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default values //FIXME
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
          sprintf(command_buffer,"ped_run -c %d -s %04x -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = ped_run(command_buffer);
          if (result == -2 || result == -3){
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("-------------------------------------------\n");
      }
    }

    pt_printsend("Time to do the cable business\n");
    pt_printsend("ECL output --> EXT PED (long cable)\n");
    pt_printsend("TTL input --> Global trigger)\n");
    pt_printsend("Hit enter when ready\n");

    read_from_tut(comments);

    // MTC_INIT
    do {
      sprintf(command_buffer,"mtc_init -x");
      result = mtc_init(command_buffer);
      if (result == -2 || result == -3){
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default + vbal values //FIXME
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // CRATE_INIT with default + vbal + tdisc values //FIXME
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
          result = set_ttot(command_buffer);
          if (result == -2 || result == -3){
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    pt_printsend("Time to remove cables\n");
    pt_printsend("Hit enter when ready\n");

    read_from_tut(comments);


    // CRATE_INIT with default + vbal + tdisc values //FIXME
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default + vbal + tdisc values //FIXME
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
        return;
      }
    } while (result != 0);
    while (sbc_lock != 0){}
    pt_printsend("-------------------------------------------\n");


    // CRATE_INIT with default + vbal + tdisc + tcmos values //FIXME
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -B -D -C",i,arg.slot_mask[i]);
          result = crate_init(command_buffer);
          if (result == -2 || result == -3){
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
          sprintf(command_buffer,"zdisc -c %d -s %04x -o 0 -r 100 -d -E %s",i,arg.slot_mask[i],ecal_id);
          result = zdisc(command_buffer);
          if (result == -2 || result == -3){
            return;
          }
        } while (result != 0);
        while (xl3_lock[i] != 0){}
        pt_printsend("------------------------------------------\n");
      }
    }


    // DO NOISE RUN


    pt_printsend("-------------------------------------------\n");
    pt_printsend("ECAL finished.\n");


  }else{
    sprintf(ecal_id,"%s",arg.ecal_id);
  }


  if (arg.update_hwdb){
    pt_printsend("Now updating FEC database with test results\n");



    char get_db_address[500];
    for (i=0;i<19;i++){
      if ((0x1<<i) & arg.crate_mask){
        for (j=0;j<16;j++){
          if ((0x1<<j) & arg.slot_mask[i]){
            /*
               sprintf(get_db_address,"%s/%s/%s/get_fec?startkey=[%d,%d]&endkey=[%d,%d]",FECDB_SERVER,FECDB_BASE_NAME,FECDB_VIEWDOC,i,j,i,j);
               pouch_request *hw_response = pr_init();
               pr_set_method(hw_response, GET);
               pr_set_url(hw_response, get_db_address);
               pr_do(hw_response);
               if (hw_response->httpresponse != 200){
               pt_printsend("Unable to connect to database. error code %d\n",(int)hw_response->httpresponse);
               return;
               }
               JsonNode *full_doc = json_decode(hw_response->resp.data);
               JsonNode* totalrows = json_find_member(full_doc,"total_rows");
               JsonNode* doc;
               if ((int)json_get_number(totalrows) == 0){
            // no entry for this slot exists yet, create it
            create_fec_db_doc(i,j,doc);
            }else{
            JsonNode* hw_rows = json_find_member(full_doc,"rows");
            JsonNode* one_row = json_find_element(hw_rows,0);
            doc = json_find_member(one_row,"value");
            }

            int crate = (int)json_get_number(json_find_member(doc,"crate"));
            int card = (int)json_get_number(json_find_member(doc,"card"));
            if (crate != i || card != j){
            pt_printsend("Database error : incorrect crate or card num (found %d,%d instead of %d,%d)\n",crate,card,i,j);
            return;
            }
             */

            // lets generate the ecal document
            JsonNode *doc;
            create_fec_db_doc(i,j,doc,&thread_fdset);


            sprintf(get_db_address,"%s/%s/%s/get_ecal?startkey=\"%s\"&endkey=\"%s\"",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,ecal_id,ecal_id);
            pouch_request *ecal_response = pr_init();
            pr_set_method(ecal_response, GET);
            pr_set_url(ecal_response, get_db_address);
            pr_do(ecal_response);
            if (ecal_response->httpresponse != 200){
              pt_printsend("Unable to connect to database. error code %d\n",(int)ecal_response->httpresponse);
              return;
            }
            JsonNode *ecalfull_doc = json_decode(ecal_response->resp.data);
            JsonNode *ecaltotalrows = json_find_member(ecalfull_doc,"total_rows");
            if ((int)json_get_number(ecaltotalrows) == 0){
              pt_printsend("No documents for this ECAL yet! (id %s)\n",ecal_id);
              return;
            }
            JsonNode *ecal_rows = json_find_member(ecalfull_doc,"rows");
            int k;
            for (k=0;k<(int)json_get_number(ecaltotalrows);k++){
              JsonNode *ecalone_row = json_find_element(ecal_rows,j);
              JsonNode *test_doc = json_find_member(ecalone_row,"value");
              printf("test type is %s\n",json_get_string(json_find_member(test_doc,"type")));
              add_ecal_test_results(doc,test_doc);
            }

            post_fec_db_doc(i,j,doc);

            json_delete(doc); // only delete the head node
            json_delete(ecalfull_doc);
            pr_free(ecal_response);
          }
        }
      }
    }


  }

  pt_printsend("-------------------------------------------\n");



  running_ecal = 0;
  unthread_and_unlock(1,arg.crate_mask,arg.thread_num); 
}

