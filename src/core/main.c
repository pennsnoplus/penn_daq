#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "pouch.h"
#include "packet_types.h"

#include "process_packet.h"
#include "daq_utils.h"
#include "net.h"
#include "net_utils.h"
#include "main.h"

int main(int argc, char *argv[])
{
  // set up a signal handler to handle ctrl-C
  (void) signal(SIGINT, sigint_func);

  PENN_DAQ_ROOT = getenv("PENN_DAQ_ROOT");
  if (PENN_DAQ_ROOT == NULL){
    printf("You need to set the environment variable PENN_DAQ_ROOT to the penn_daq directory\n");
    return -1;
  }
  CURRENT_LOCATION = ABOVE_GROUND_TESTSTAND;
  write_log = 0;
  start_time = 0;
  end_time = 0;
  last_print_time = 0;
  megabundle_count = 0;
  recv_bytes = 0;
  recv_fake_bytes = 0;
  running_macro = 0;
  reading_from_tut = 0;
  running_final_test = 0;
  running_ecal = 0;
  int i,j;
  for (i=0;i<MAX_THREADS;i++)
    thread_done[i] = 0;
  pthread_mutex_init(&printsend_buffer_lock,NULL);
  pthread_mutex_init(&main_fdset_lock,NULL);
  pthread_mutex_init(&socket_lock,NULL);
  pthread_mutex_init(&final_test_cmd_lock,NULL);
  pthread_cond_init(&final_test_cmd_cv,NULL);
  memset(printsend_buffer,'\0',sizeof(printsend_buffer));
  memset(final_test_cmd,'\0',sizeof(final_test_cmd));
  for (i=0;i<MAX_XL3_CON;i++){
    command_number[i] = 0;
    multifc_buffer_full[i] = 0;
  }
  memset(crate_config,0,sizeof(crate_config));

  // update configuration from config file
  int rc = read_configuration_file();
  if (rc < 0){
    printf("error reading configuration, exiting\n");
    return -1;
  }
    sprintf(DB_SERVER,"http://%s:%s@%s:%s",DB_USERNAME,DB_PASSWORD,DB_ADDRESS,DB_PORT);

  // get command line options
  int c;
  int option_index = 0;
  static struct option long_options[] =
  {
    /* These options set a flag. */
    /* These options don't set a flag. */
    {"log", no_argument, 0, 'l'},
    {"help", no_argument, 0, 'h'},
    {"penn", no_argument, 0, 'p'},
    {"aboveground", no_argument, 0, 'a'},
    {"underground", no_argument, 0, 'u'},
    {"macro",required_argument, 0, 'm'},
    {0, 0, 0, 0}
  };
  while ((c = getopt_long(argc, argv, "lhpau", long_options, &option_index)) != -1){
    switch (c){
      case 0:
        break;
      case 'l':
        printsend("Starting to log!\n");
        start_logging();
        break;
      case 'h':
        printsend("usage: %s [-l/--log] [-m (macro file)] [-p/--penn|-a/--aboveground|-u/--underground]", argv[0]);
        printsend("            or\n");
        printsend("       %s [-h/--help]\n", argv[0]);
        printsend("For more help, read the README\n");
        exit(0);
        break;
      case 'p':
        CURRENT_LOCATION = PENN_TESTSTAND;
        break;
      case 'a':
        CURRENT_LOCATION = ABOVE_GROUND_TESTSTAND;
        break;
      case 'u':
        CURRENT_LOCATION = UNDERGROUND;
        break;
      case 'm':
        parse_macro(optarg);
        break;
      case '?':
        /* getopt_long already printed an error message. */
        break;
      default:
        abort();
    }
  }


  // make sure the database is up and running
  pouch_request *pr = pr_init();
  pr = db_get(pr, DB_SERVER, DB_BASE_NAME);
  pr_do(pr);
  if(pr->httpresponse != 200){
    printsend("Unable to connect to database. error code %d\n",(int)pr->httpresponse);
    printsend("CURL error code: %d\n", pr->curlcode);
    exit(0);
  }
  else{
    printsend("Connected to database: http response code %d\n",(int)pr->httpresponse);
  }
  pr_free(pr);

  printsend("current location is %d\n",CURRENT_LOCATION);

  // set up sockets to listen for new connections
  setup_sockets();
  printsend("\nNAME\t\tPORT#\n");
  printsend("XL3s\t\t%d-%d\n", XL3_PORT, XL3_PORT+MAX_XL3_CON-1);
  printsend("SBC/MTC\t\t%d\n", SBC_PORT);
  printsend("CONTROLLER\t%d\n", CONT_PORT);
  printsend("VIEWERs\t\t%d\n\n", VIEW_PORT);
  printsend("waiting for connections...\n");


  // main loop
  while(1){
    // first send messages queued up by threads
    send_queued_msgs();
    // next continue macro if one is running
    if (running_macro)
      run_macro();
    // reset our fdsets
    pthread_mutex_lock(&main_fdset_lock);
    pthread_mutex_lock(&socket_lock);
    fd_set temp = main_fdset;
    if (sbc_lock && rw_sbc_fd > 0)
      FD_CLR(rw_sbc_fd,&temp);
    for (i=0;i<MAX_XL3_CON;i++){
      if ((xl3_lock[i] == 1) && (rw_xl3_fd[i] > 0) && (reading_from_tut == 0))
        FD_CLR(rw_xl3_fd[i],&temp);
    }
      
    if (cont_lock && rw_cont_fd > 0)
      FD_CLR(rw_cont_fd,&temp);
    main_readable_fdset = temp;
    main_writeable_fdset = temp;
    // free any threads that have finished
    cleanup_threads();
    // now we do the select
    int s = select(fdmax+1,&main_readable_fdset,&main_writeable_fdset,NULL,0);
    pthread_mutex_unlock(&main_fdset_lock);
    if (s == -1){
      printf("Select error in main loop\n");
      sigint_func(SIGINT);
    }else if (s > 0){
      // we've got something to read or write
      // loop over all the fds in our set, check which are readable and read them
      int cur_fd;
      for (cur_fd = 0;cur_fd <= fdmax;cur_fd++){
        // check if its readable 
        if (FD_ISSET(cur_fd,&main_readable_fdset)){
          read_socket(cur_fd);
        } // end if fd is readable
      } // end loop over all fds
    }// end if select returns > 0
    // let functions know its ok to change which sockets this loop checks now
    pthread_mutex_unlock(&socket_lock);
  } // end main loop

  sigint_func(SIGINT);
  return 0;
}
