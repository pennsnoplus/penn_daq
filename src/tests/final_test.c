#include <pthread.h>
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
#include "final_test.h"

int final_test(char *buffer)
{
  final_test_t *args;
  args = malloc(sizeof(final_test_t));

  args->crate_num = 2;
  args->slot_mask = 0x80;
  args->tub_tests = 1;
  args->skip = 0;

  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->crate_num = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL," ")) != NULL)
          args->slot_mask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'p'){CURRENT_LOCATION = PENN_TESTSTAND;
      }else if (words[1] == 'a'){CURRENT_LOCATION = ABOVE_GROUND_TESTSTAND;
      }else if (words[1] == 'u'){CURRENT_LOCATION = UNDERGROUND;
      }else if (words[1] == 't'){args->tub_tests = 0;
      }else if (words[1] == 'q'){args->skip = 1;
      }else if (words[1] == 'h'){
        pt_printsend("Usage: final_test -c [crate num (int)] "
            "-s [slot mask (hex)] -p (penn test stand) -a (above ground test stand) "
            "-u (underground)\n");
        free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }

  pthread_t *new_thread;
  int thread_num = thread_and_lock(2,(0x1<<args->crate_num),&new_thread);
  if (thread_num < 0){
    free(args);
    return -1;
  }

  args->thread_num = thread_num;
  pthread_create(new_thread,NULL,pt_final_test,(void *)args);
  return 0; 
}


void *pt_final_test(void *args)
{
  final_test_t arg = *(final_test_t *) args; 
  free(args);

  int i;
  // once this is set we can no longer send other commands
  running_final_test = 1;
  // now we can unlock our things, since nothing else should use them
  sbc_lock = 0;
  xl3_lock[arg.crate_num] = 0;

  fd_set thread_fdset;
  FD_ZERO(&thread_fdset);
  FD_SET(rw_xl3_fd[arg.crate_num],&thread_fdset);

  char ft_ids[16][250];
  char id_string[16*250];
  JsonNode *ft_docs[16];

  char comments[1000];
  memset(comments,'\0',1000);
  char command_buffer[100];

  system("clear");
  pt_printsend("------------------------------------------\n");
  pt_printsend("Welcome to final test!\nHit enter to start\n");
  read_from_tut(comments);
  pt_printsend("------------------------------------------\n");

  do {
    sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
  } while (crate_init(command_buffer) != 0);
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
  } while (mtc_init(command_buffer) != 0);

  while (sbc_lock != 0 || xl3_lock[arg.crate_num] != 0){}
  pt_printsend("------------------------------------------\n");
  pt_printsend("If any boards could not initialize properly, type \"quit\" now "
      "to exit the test.\n Otherwise hit enter to continue.\n");
  read_from_tut(comments);
  if (strncmp("quit",comments,4) == 0){
    pt_printsend("Exiting final test\n");
    running_final_test = 0;
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
  } while (board_id(command_buffer) != 0);
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
  } while (fec_test(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"vmon -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (vmon(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"cgt_test_1 -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (cgt_test(command_buffer) != 0);
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
    sprintf(command_buffer,"ped_run -c %d -s %04x -l 400 -u 800 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (ped_run(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}


  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"crate_cbal -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (crate_cbal(command_buffer) != 0);
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
  } while (crate_init(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"ped_run -c %d -s %04x -b -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (ped_run(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"chinj_scan -c %d -s %04x -l 0 -u 5000 -w 100 -n 10 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (chinj_scan(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  if (arg.tub_tests == 1){
    pt_printsend("You should now connect the cable to ext_ped for ttot tests\nHit enter when ready\n");
    read_from_tut(comments);
    do {
      sprintf(command_buffer,"set_ttot -c %d -s %04x -t 400 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
    } while (set_ttot(command_buffer)!= 0);
    while (xl3_lock[arg.crate_num] != 0){}
  }

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"crate_init -c %d -s %04x -t",arg.crate_num,arg.slot_mask);
  } while (crate_init(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  if (arg.tub_tests == 1){
    do {
      sprintf(command_buffer,"get_ttot -c %d -s %04x -t 440 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
    } while (get_ttot(command_buffer) != 0);
    while (xl3_lock[arg.crate_num] != 0){}
  }

  pt_printsend("-------------------------------------------\n");
  pt_printsend("You should now disconnect the cable to ext_ped\nHit enter when ready\n");
  read_from_tut(comments);
  do {
    sprintf(command_buffer,"disc_check -c %d -s %04x -n 500000 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (disc_check(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"cmos_m_gtvalid -c %d -s %04x -g 400 -n -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (cmos_m_gtvalid(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  // would put see_reflections here

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"zdisc -c %d -s %04x -o 0 -r 100 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (zdisc(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"mtc_init");
  } while (mtc_init(command_buffer) != 0);
  while (sbc_lock != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
  } while (crate_init(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do{
    sprintf(command_buffer,"mb_stability_test -c %d -s %04x -n 50 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (mb_stability_test(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"fifo_test -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (fifo_test(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"crate_init -c %d -s %04x -X",arg.crate_num,arg.slot_mask);
  } while (crate_init(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"cald_test -c %d -s %04x -l 750 -u 3500 -n 200 -d -# %s",arg.crate_num,arg.slot_mask,id_string);
  } while (cald_test(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      do {

        pt_printsend("-------------------------------------------\n");
        do {
          sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
        } while (crate_init(command_buffer) != 0);
        while (xl3_lock[arg.crate_num] != 0){}

        pt_printsend("-------------------------------------------\n");

        sprintf(command_buffer,"mem_test -c %d -s %d -d -# %s",arg.crate_num,i,ft_ids[i]);
      } while (mem_test(command_buffer) != 0);
      while (xl3_lock[arg.crate_num] != 0){}
    }
  }

  pt_printsend("-------------------------------------------\n");
  do {
    sprintf(command_buffer,"crate_init -c %d -s %04x -x",arg.crate_num,arg.slot_mask);
  } while (crate_init(command_buffer) != 0);
  while (xl3_lock[arg.crate_num] != 0){}

  pt_printsend("-------------------------------------------\n");
  pt_printsend("Ready for see_refl test. Hit enter to begin or type \"skip\" to end the final test.\n");
  read_from_tut(comments);
  if (strncmp("skip",comments,4) != 0){
    do {
      sprintf(command_buffer,"see_refl -c %d -s %04x -d -# %s",arg.crate_num,arg.slot_mask,id_string);
    } while (see_refl(command_buffer) != 0);
    while (xl3_lock[arg.crate_num] != 0){}
  }



  pt_printsend("-------------------------------------------\n");
  pt_printsend("Final test finished. Now updating the database.\n");

  // update the database
  for (i=0;i<16;i++){
    if ((0x1<<i) & arg.slot_mask){
      json_append_member(ft_docs[i],"type",json_mkstring("final_test"));
      pthread_mutex_lock(&socket_lock);
      xl3_lock[arg.crate_num] = 1; // we gotta lock this shit down before we call post_doc
      pthread_mutex_unlock(&socket_lock);
      post_debug_doc_with_id(arg.crate_num, i, ft_ids[i], ft_docs[i],&thread_fdset);
      json_delete(ft_docs[i]);
    }
  }

  pt_printsend("-------------------------------------------\n");



  running_final_test = 0;
  unthread_and_unlock(1,(0x1<<arg.crate_num),arg.thread_num); 
}

