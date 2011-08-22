#include <getopt.h>
#include <signal.h>
#include <stdlib.h>

#include "pouch.h"

#include "daq_utils.h"
#include "net_utils.h"
#include "main.h"

int main(int argc, char *argv[])
{
  // set up a signal handler to handle ctrl-C
  (void) signal(SIGINT, sigint_func);

  current_location = 0;
  write_log = 0;

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
        printsend("usage: %s [-l/--log] [-p/--penn|-a/--aboveground|-u/--underground]", argv[0]);
        printsend("            or\n");
        printsend("       %s [-h/--help]\n", argv[0]);
        printsend("For more help, read the README\n");
        exit(0);
        break;
      case 'p':
        current_location = 2;
        break;
      case 'a':
        current_location = 0;
        break;
      case 'u':
        current_location = 1;
        break;
      case '?':
        /* getopt_long already printed an error message. */
        break;
      default:
        abort();
    }
  }

  printsend("current location is %d\n",current_location);

  // make sure the database is up and running
  pouch_request *pr = pr_init();
  pr = db_get(pr, DB_SERVER, DB_BASE_NAME);
  pr_do(pr);
  if(pr->httpresponse != 200){
    printsend("Unable to connect to database. error code %d\n",(int)pr->httpresponse);
    printsend("CURL error code: %d\n", pr->curlcode);
    //exit(0);
  }
  else{
    printsend("Connected to database: http response code %d\n",(int)pr->httpresponse);
  }
  pr_free(pr);

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
    // reset our fdsets
    main_readable_fdset = main_fdset;
    main_writeable_fdset = main_fdset;
    // now we do the select
    int s = select(fdmax+1,&main_readable_fdset,&main_writeable_fdset,NULL,0);
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
    } // end if select returns > 0
  } // end main loop

  sigint_func(SIGINT);
  return 0;
}
