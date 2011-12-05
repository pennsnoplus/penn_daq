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

      }else if (words[1] == 'h'){
        pt_printsend("Usage: ecal -c [crate mask (hex)] "
            "-s [slot mask for all crates (hex)] "
            "-00...-18 [slot mask for that crate (hex)]");
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

  int i;
  int result;
  // once this is set we can no longer send other commands
  running_ecal = 1;
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

  char comments[1000];
  memset(comments,'\0',1000);
  char command_buffer[100];
  memset(command_buffer,'\0',100);

  system("clear");
  pt_printsend("------------------------------------------\n");
  pt_printsend("Welcome to ECAL+!\n");
  pt_printsend("------------------------------------------\n");
  pt_printsend("\nYou have selected the following slots:\n\n");
  for (i=0;i<6;i++){
    pt_printsend("crate %d-0x%08x\tcrate %d-0x%08x\tcrate %d-0x%08x\n",
        i*3,arg.slot_mask[i*3],i*3+1,arg.slot_mask[i*3+1],i*3+2,arg.slot_mask[i*3+2]);
  }
  pt_printsend("crate 18-0x%08x\n\n",arg.slot_mask[18]);
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


  char ecal_id[250];
  get_new_id(ecal_id);

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
        sprintf(command_buffer,"crate_init -c %d -s %04x -b",i,arg.slot_mask[i]);
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
        sprintf(command_buffer,"crate_init -c %d -s %04x -b",i,arg.slot_mask[i]);
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
        sprintf(command_buffer,"crate_init -c %d -s %04x -b -t",i,arg.slot_mask[i]);
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
        sprintf(command_buffer,"crate_init -c %d -s %04x -b -t",i,arg.slot_mask[i]);
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
        sprintf(command_buffer,"crate_init -c %d -s %04x -b -t",i,arg.slot_mask[i]);
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
        sprintf(command_buffer,"crate_init -c %d -s %04x -b -t",i,arg.slot_mask[i]);
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





  /*

     do {
     sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
     result = crate_init(command_buffer);
     if (result == -2 || result == -3){
     return;
     }
     } while (result != 0);
     while (xl3_lock[arg.crate_num] != 0){}
     pt_printsend("------------------------------------------\n");


     while (rw_sbc_fd <= 0){
     pt_printsend("Attempting to connecting to sbc\n");
     do {
     sprintf(command_buffer,"sbc_control");
     } while (sbc_control(command_buffer) != 0);
     while (sbc_lock != 0){}
     }
     pt_printsend("------------------------------------------\n");

     do {
     sprintf(command_buffer,"mtc_init -x");
     result = mtc_init(command_buffer);
     if (result == -2 || result == -3){
     return;
     }

     } while (result != 0);

     while (sbc_lock != 0 || xl3_lock[arg.crate_num] != 0){}
     pt_printsend("------------------------------------------\n");
     pt_printsend("If any boards could not initialize properly, type \"quit\" now "
     "to exit the test.\n Otherwise hit enter to continue.\n");
     read_from_tut(comments);
     if (strncmp("quit",comments,4) == 0){
     pt_printsend("Exiting final test\n");
     running_ecal = 0;
     unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num); 
     return;
     }

  // set up the final test documents
  for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
  get_new_id(ft_ids[i]);
  ft_docs[i] = json_mkobject();
  sprintf(id_string+strlen(id_string),"%s ",ft_ids[i]);
  pt_printsend(".%s.\n",id_string);
  }
  }

  pt_printsend("Now starting board_id\n");
  do {
  sprintf(command_buffer,"board_id -c %d -s %04x",arg.crate_num,arg.slot_mask);
  result = board_id(command_buffer);
  if (result == -2 || result == -3){
  return;
  }

  } while (result != 0);
  while (xl3_lock[arg.crate_num] != 0){}
  pt_printsend("-------------------------------------------\n");

  if (arg.skip == 0){

  for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
  pt_printsend("Please enter any comments for slot %i motherboard now.\n",i);
  read_from_tut(comments);
json_append_member(ft_docs[i],"fec_comments",json_mkstring(comments));
pt_printsend("Has this slot been refurbished? (y/n)\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"refurbished",json_mkbool(comments[0] == 'y'));
pt_printsend("Has this slot been cleaned? (y/n)\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"cleaned",json_mkbool(comments[0] == 'y'));
pt_printsend("Time to measure resistance across analog outs and cmos address lines. For the cmos address lines"
    "it's easier if you do it during the fifo mod\n");
read_from_tut(comments);
json_append_member(ft_docs[i],"analog_out_res",json_mkstring(comments));
pt_printsend("Please enter any comments for slot %i db 0 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db0_comments",json_mkstring(comments));
pt_printsend("Please enter any comments for slot %i db 1 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db1_comments",json_mkstring(comments));
pt_printsend("Please enter any comments for slot %i db 2 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db2_comments",json_mkstring(comments));
pt_printsend("Please enter any comments for slot %i db 3 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db3_comments",json_mkstring(comments));
pt_printsend("Please enter dark matter measurements for slot %i db 0 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db0_dark_matter",json_mkstring(comments));
pt_printsend("Please enter dark matter measurements for slot %i db 1 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db1_dark_matter",json_mkstring(comments));
pt_printsend("Please enter dark matter measurements for slot %i db 2 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db2_dark_matter",json_mkstring(comments));
pt_printsend("Please enter dark matter measurements for slot %i db 3 now.\n",i);
read_from_tut(comments);
json_append_member(ft_docs[i],"db3_dark_matter",json_mkstring(comments));
}
}


pt_printsend("Enter N100 DC offset\n");
read_from_tut(comments);
for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
    json_append_member(ft_docs[i],"dc_offset_n100",json_mkstring(comments));
  }
}
pt_printsend("Enter N20 DC offset\n");
read_from_tut(comments);
for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
    json_append_member(ft_docs[i],"dc_offset_n20",json_mkstring(comments));
  }
}
pt_printsend("Enter esum hi DC offset\n");
read_from_tut(comments);
for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
    json_append_member(ft_docs[i],"dc_offset_esumhi",json_mkstring(comments));
  }
}
pt_printsend("Enter esum lo DC offset\n");
read_from_tut(comments);
for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
    json_append_member(ft_docs[i],"dc_offset_esumlo",json_mkstring(comments));
  }
}

pt_printsend("Thank you. Please hit enter to continue with the rest of final test. This may take a while.\n");
read_from_tut(comments);
}

// starting the tests
pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"fec_test -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = fec_test(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"vmon -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = vmon(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"cgt_test_1 -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = cgt_test(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

//////////////////////////////////////////////////////////////
// now gotta turn pulser on and off to get rid of garbage
//sprintf(command_buffer,"readout_add_mtc -c %d -f 0",arg.crate_num);
//readout_add_mtc(command_buffer);
//sprintf(command_buffer,"stop_pulser");
//stop_pulser(command_buffer);
//////////////////////////////////////////////////////////////

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"ped_run -c %d -s %04x -l 300 -u 1000 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = ped_run(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}


pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"crate_cbal -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = crate_cbal(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

//////////////////////////////////////////////////////////////
// now gotta turn pulser on and off to get rid of garbage
//sprintf(command_buffer,"readout_add_mtc -c %d -f 0",arg.crate_num);
//readout_add_mtc(command_buffer);
//sprintf(command_buffer,"stop_pulser");
//stop_pulser(command_buffer);
//////////////////////////////////////////////////////////////

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"crate_init -c %d -s %04x -b",arg.crate_num,arg.slot_mask);
  result = crate_init(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"ped_run -c %d -s %04x -b -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = ped_run(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"chinj_scan -c %d -s %04x -l 0 -u 5000 -w 100 -n 10 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = chinj_scan(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
if (arg.tub_tests == 1){
  pt_printsend("You should now connect the cable to ext_ped for ttot tests\nHit enter when ready\n");
  read_from_tut(comments);
  do {
    sprintf(command_buffer,"set_ttot -c %d -s %04x -t 400 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
    result = set_ttot(command_buffer);
    if (result == -2 || result == -3){
      return;
    }

  } while (result != 0);
  while (xl3_lock[arg.crate_num] != 0){}
}




pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"crate_init -c %d -s %04x -t",arg.crate_num,arg.slot_mask);
  result = crate_init(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
if (arg.tub_tests == 1){
  do {
    sprintf(command_buffer,"get_ttot -c %d -s %04x -t 390 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
    result = get_ttot(command_buffer);
    if (result == -2 || result == -3){
      return;
    }

  } while (result != 0);
  while (xl3_lock[arg.crate_num] != 0){}
}

pt_printsend("-------------------------------------------\n");
pt_printsend("You should now disconnect the cable to ext_ped\nHit enter when ready\n");
read_from_tut(comments);
do {
  sprintf(command_buffer,"disc_check -c %d -s %04x -n 500000 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = disc_check(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"cmos_m_gtvalid -c %d -s %04x -g 400 -n -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = cmos_m_gtvalid(command_buffer); 
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

// would put see_reflections here

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"zdisc -c %d -s %04x -o 0 -r 100 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = zdisc(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"mtc_init");
  result = mtc_init(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (sbc_lock != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
  result = crate_init(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do{
  sprintf(command_buffer,"mb_stability_test -c %d -s %04x -n 50 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = mb_stability_test(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"fifo_test -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = fifo_test(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"crate_init -c %d -s %04x -X",arg.crate_num,arg.slot_mask);
  result = crate_init(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"cald_test -c %d -s %04x -l 750 -u 3500 -n 200 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  result = cald_test(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
    do {

      pt_printsend("-------------------------------------------\n");
      do {
        sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
        result = crate_init(command_buffer);
        if (result == -2 || result == -3){
          return;
        }

      } while (result != 0);
      while (xl3_lock[arg.crate_num] != 0){}

      pt_printsend("-------------------------------------------\n");

      sprintf(command_buffer,"mem_test -c %d -s %d -d -# %s",arg.crate_num,i,ft_ids[i]);
      result = mem_test(command_buffer);
      if (result == -2 || result == -3){
        return;
      }

    } while (result != 0);
    while (xl3_lock[arg.crate_num] != 0){}
  }
}

pt_printsend("-------------------------------------------\n");
do {
  sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
  result = crate_init(command_buffer);
  if (result == -2 || result == -3){
    return;
  }

} while (result != 0);
while (xl3_lock[arg.crate_num] != 0){}

pt_printsend("-------------------------------------------\n");
pt_printsend("Ready for see_refl test. Hit enter to begin or type \"skip\" to end the final test.\n");
read_from_tut(comments);
if (strncmp("skip",comments,4) != 0){
  do {
    sprintf(command_buffer,"see_refl -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
    result = see_refl(command_buffer);
    if (result == -2 || result == -3){
      return;
    }

  } while (result != 0);
  while (xl3_lock[arg.crate_num] != 0){}
}



pt_printsend("-------------------------------------------\n");
pt_printsend("Final test finished. Now updating the database.\n");

// update the database
for (i=0;i<16;i++){
  if ((0x1<<i) & arg.slot_mask){
    json_append_member(ft_docs[i],"type",json_mkstring("ecal"));
    pthread_mutex_lock(&socket_lock);
    xl3_lock[arg.crate_num] = 1; // we gotta lock this shit down before we call post_doc
    pthread_mutex_unlock(&socket_lock);
    post_debug_doc_with_id(arg.crate_num, i, ft_ids[i], ft_docs[i],&thread_fdset);
    json_delete(ft_docs[i]);
  }
}

pt_printsend("-------------------------------------------\n");



running_ecal = 0;
unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num); 
*/
}

