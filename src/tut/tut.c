/*
 ** tut.c -- a client with tab completion
 **	designed to replace TELNET for the SNO+
 ** experiment. should really only be used
 ** during the testing phase, but hopefully
 ** it will be useful for a while.
 ** Based extensively off of the client on beej.us
 ** and the tab completion found somewhere else, I
 ** can't really remember.
 ** 
 ** Written by Peter Downs (August 4th, 2010)
 **	
 */
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>


#define MAXDATASIZE 1440 // max number of bytes we can get at once 
#define CONFIG_FILE_LOC "data/config.cfg"

extern char *getwd ();
int sockfd, numbytes;
pid_t pid;
int CONT_PORT;
char CONT_CMD_ACK[100];
char CONT_CMD_BSY[100];

/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
    char *name;			/* User printable name of the function. */
    Function *func;		/* Function to call to do the job. */
    char *doc;			/* Documentation for this function.  */
} COMMAND;

int com_help();
/* to add a command, just follow the structure:
   { $command_name_string, (Function *)NULL, (char *)NULL },
 */
COMMAND commands[] = {
    //{ "help", com_help, (char *)NULL },
    //_!_begin_commands_!_
    { "exit", (Function *)NULL, (char *)NULL },
    { "help", (Function *)NULL, (char *)NULL },
    { "print_connected", (Function *)NULL, (char *)NULL },
    { "stop_logging", (Function *)NULL, (char *)NULL },
    { "set_location", (Function *)NULL, (char *)NULL },
    { "start_logging", (Function *)NULL, (char *)NULL },
    { "sbc_control", (Function *)NULL, (char *)NULL },
    { "clear_screen", (Function *)NULL, (char *)NULL },
    { "reset_speed", (Function *)NULL, (char *)NULL },
    { "kill_threads", (Function *)NULL, (char *)NULL },
    { "run_macro", (Function *)NULL, (char *)NULL },
    { "stop_macro", (Function *)NULL, (char *)NULL },
    { "xr", (Function *)NULL, (char *)NULL },
    { "xw", (Function *)NULL, (char *)NULL },
    { "fr", (Function *)NULL, (char *)NULL },
    { "fw", (Function *)NULL, (char *)NULL },
    { "mr", (Function *)NULL, (char *)NULL },
    { "mw", (Function *)NULL, (char *)NULL },
    { "debugging_on", (Function *)NULL, (char *)NULL },
    { "debugging_off", (Function *)NULL, (char *)NULL },
    { "change_mode", (Function *)NULL, (char *)NULL },
    { "crate_init", (Function *)NULL, (char *)NULL },
    { "mtc_init", (Function *)NULL, (char *)NULL },
    { "sm_reset", (Function *)NULL, (char *)NULL },
    { "read_local_voltage", (Function *)NULL, (char *)NULL },
    { "hv_readback", (Function *)NULL, (char *)NULL },
    { "xl3_rw", (Function *)NULL, (char *)NULL },
    { "xl3_queue_rw", (Function *)NULL, (char *)NULL },
    { "set_alarm_dac", (Function *)NULL, (char *)NULL },
    { "load_dac", (Function *)NULL, (char *)NULL },
    { "read_bundle", (Function *)NULL, (char *)NULL },
    { "setup_chinj", (Function *)NULL, (char *)NULL },
    { "mtc_read", (Function *)NULL, (char *)NULL },
    { "mtc_write", (Function *)NULL, (char *)NULL },
    { "set_mtca_thresholds", (Function *)NULL, (char *)NULL },
    { "set_gt_mask", (Function *)NULL, (char *)NULL },
    { "set_gt_crate_mask", (Function *)NULL, (char *)NULL },
    { "set_ped_crate_mask", (Function *)NULL, (char *)NULL },
    { "run_pedestals_end_mtc", (Function *)NULL, (char *)NULL },
    { "run_pedestals_end_crate", (Function *)NULL, (char *)NULL },
    { "run_pedestals_end", (Function *)NULL, (char *)NULL },
    { "run_pedestals_mtc", (Function *)NULL, (char *)NULL },
    { "run_pedestals_crate", (Function *)NULL, (char *)NULL },
    { "run_pedestals", (Function *)NULL, (char *)NULL },
    { "enable_pulser", (Function *)NULL, (char *)NULL },
    { "disable_pulser", (Function *)NULL, (char *)NULL },
    { "enable_pedestal", (Function *)NULL, (char *)NULL },
    { "disable_pedestal", (Function *)NULL, (char *)NULL },
    { "set_pulser_freq", (Function *)NULL, (char *)NULL },
    { "send_softgt", (Function *)NULL, (char *)NULL },
    { "multi_softgt", (Function *)NULL, (char *)NULL },
    { "trigger_scan", (Function *)NULL, (char *)NULL },
    { "see_refl", (Function *)NULL, (char *)NULL },
    { "fec_test", (Function *)NULL, (char *)NULL },
    { "mem_test", (Function *)NULL, (char *)NULL },
    { "vmon", (Function *)NULL, (char *)NULL },
    { "board_id", (Function *)NULL, (char *)NULL },
    { "ped_run", (Function *)NULL, (char *)NULL },
    { "zdisc", (Function *)NULL, (char *)NULL },
    { "crate_cbal", (Function *)NULL, (char *)NULL },
    { "cgt_test", (Function *)NULL, (char *)NULL },
    { "cmos_m_gtvalid", (Function *)NULL, (char *)NULL },
    { "cald_test", (Function *)NULL, (char *)NULL },
    { "get_ttot", (Function *)NULL, (char *)NULL },
    { "set_ttot", (Function *)NULL, (char *)NULL },
    { "fifo_test", (Function *)NULL, (char *)NULL },
    { "disc_check", (Function *)NULL, (char *)NULL },
    { "mb_stability_test", (Function *)NULL, (char *)NULL },
    { "chinj_scan", (Function *)NULL, (char *)NULL },
    { "final_test", (Function *)NULL, (char *)NULL },
    { "find_noise", (Function *)NULL, (char *)NULL },
    { "ecal", (Function *)NULL, (char *)NULL },
    //_!_end_commands_!_
    { (char *)NULL, (Function *)NULL, (char*)NULL }
};

int read_configuration_file()
{
  FILE *config_file;
  char filename[500];
  char *PENN_DAQ_ROOT = getenv("PENN_DAQ_ROOT");
  if (PENN_DAQ_ROOT == NULL){
    printf("You need to set the environment variable PENN_DAQ_ROOT to the penn_daq directory\n");
    exit(-1);
  }
  sprintf(filename,"%s/%s",PENN_DAQ_ROOT,CONFIG_FILE_LOC);
  config_file = fopen(filename,"r");
  int i,n = 0;
  char line_in[100][100];
  while (fscanf(config_file,"%s",line_in[n]) == 1){
    n++;
  }
  for (i=0;i<n;i++){
    char *var_name,*var_value;
    var_name = strtok(line_in[i],"=");
    if (var_name != NULL){
      var_value = strtok(NULL,"=");
      if (var_name[0] != '#' && var_value != NULL){
        if (strcmp(var_name,"CONT_PORT")==0){
          CONT_PORT = atoi(var_value);
        }else if (strcmp(var_name,"CONT_CMD_ACK")==0){
          strcpy(CONT_CMD_ACK,var_value);
        }else if (strcmp(var_name,"CONT_CMD_BSY")==0){
          strcpy(CONT_CMD_BSY,var_value);
        }
      }
    }
  }
  fclose(config_file);
  printf("done reading config\n");
  return 0; 
}



// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void * xmalloc (int size)
{
  void *buf;

  buf = malloc (size);
  if (!buf) {
    fprintf (stderr, "Error: Out of memory. Exiting.'n");
    exit (1);
  }

  return buf;
}

char *dupstr(char* s){
  char *r;
  r = xmalloc (strlen (s) + 1);
  strcpy (r, s);
  return (r);
}
/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND *find_command(char *name){
  register int i;
  for (i = 0; commands[i].name; i++){
    if (strcmp (name, commands[i].name) == 0)
      return (&commands[i]);
  }
  return ((COMMAND *)NULL);
}

int execute_line(char *line){
  register int i;
  COMMAND *command;
  char *word;
  /* Isolate the command word. */
  i = 0;
  while (line[i] && isspace(line[i]) )
    i++;
  word = line + i;
  while (line[i] && !isspace (line[i]))
    i++;
  if (line[i])
    line[i++] = '\0';
  command = find_command (word);
  if (!command)
  {
    return (-1);
  }
  /* Get argument to command, if any. */
  while (isspace (line[i])){
    i++;
  }
  word = line + i;
  /* Call the function. */
  return ( (*(command->func))(word, 0) );
}

/* Strip isspace from the start and end of STRING.  Return a pointer
   into STRING. */
char *stripwhite(char *string){
  register char *s, *t;
  for (s = string; isspace (*s); s++)
    ;
  if (*s == 0)
    return (s);
  t = s + strlen (s) - 1;
  while (t > s && isspace (*t))
    t--;
  *++t = '\0';
  return s;
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */

char *command_generator ();
char **fileman_completion ();

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
initialize_readline ()
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = "FileMan";
  /* Tell the completer that we want a crack first. */
  rl_attempted_completion_function = (CPPFunction *)fileman_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */
char **fileman_completion(char *text, int start, int end){
  char **matches;
  matches = (char **)NULL;
  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0)
    matches = rl_completion_matches(text, command_generator);
  return (matches);
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *command_generator(char *text, int state){
  static int list_index, len;
  char *name;
  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (!state)
  {
    list_index = 0;
    len = strlen (text);
  }
  /* Return the next name which partially matches from the command list. */
  while (name = commands[list_index].name)
  {
    list_index++;
    if (strncmp (name, text, len) == 0)
      return (dupstr(name));
  }
  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

/* **************************************************************** */
/*                                                                  */
/*                       FileMan Commands                           */
/*                                                                  */
/* **************************************************************** */


int com_help(char *arg){
  register int i;
  int printed = 0;
  printf("\t***************\n");
  printf("\tVALID FUNCTIONS\n");
  printf("\t***************\n");

  for (i = 0; commands[i].name; i++)
  {
    if (!*arg || (strcmp (arg, commands[i].name) == 0))
    {
      printf ("\t%s\n", commands[i].name);
      printed++;
    }
  }

  if (!printed)
  {
    printf ("No commands match `%s'.  Possibilties are:\n", arg);

    for (i = 0; commands[i].name; i++)
    {
      /* Print in six columns. */
      if (printed == 6)
      {
        printed = 0;
        printf ("\n");
      }

      printf ("%s\t", commands[i].name);
      printed++;
    }

    if (printed)
      printf ("\n");
  }
  return (0);
}

/* Return non-zero if ARG is a valid argument for CALLER, else print
   an error message and return zero. */
int valid_argument(char *caller, char *arg){
  if (!arg || !*arg)
  {
    fprintf (stderr, "%s: Argument required.\n", caller);
    return (0);
  }

  return (1);
}

void leave(int sig){
  printf("\nbeginning shutdown:\n");
  write_history(".history.txt");
  printf("\twrote ./.history.txt\n");
  close(sockfd);
  printf("\tclosed socket\n");
  kill(pid, SIGTERM);
  printf("\tkilled child process\n");
  printf("shutdown complete\n");
  exit(0);
}


int main(int argc, char *argv[])
{
  (void) signal(SIGINT,leave);

  using_history();
  read_configuration_file();
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  char port[100];
  memset(port,'\0',sizeof(port));
  sprintf(port,"%d",CONT_PORT);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (argc != 3) {
    //    fprintf(stderr,"takes two arguments: hostname port\n");
    //    exit(1);
    if ((rv = getaddrinfo("localhost", port, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }
  }else{
    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }
  }


  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
      perror("TUT: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("TUT: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "TUT: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
      s, sizeof s);
  printf("TUT: Telnet with Up arrow and Tab completion\n");
  printf("C-c to exit, \"help\" for commands\n");
  printf("CONNECTED TO  %s:%s\n", s, argv[2]);
  printf("******************************\n");


  freeaddrinfo(servinfo); // all done with this structure
  /*
##############################
#		MAIN COMMAND LOOP	 #
##############################
   */
  char command[MAXDATASIZE];
  char response[MAXDATASIZE];
  char *buf;
  int i;
  pid = fork();			// fork into two processes: one which sends data, one which prints received data
  /*
     RECEIVING LOOP
   */
  if(pid < 1){	// child process (receives incoming data and prints it)
    struct timeval moretime;
    moretime.tv_sec=0;
    moretime.tv_usec=100000;
    while(1){
      memset(response, '\0', MAXDATASIZE);	// clear the response buffer
      numbytes = recv(sockfd, response, MAXDATASIZE-1, 0);
      if (numbytes > 0){
        if(strncmp(response, CONT_CMD_ACK, strlen(CONT_CMD_ACK)) == 0){
          moretime.tv_sec=0;
          moretime.tv_usec=100000;
          write(1, "********************", 20);
          select(0, NULL, NULL, NULL, &moretime);
          write(1, "\r                            \r", 30);
        }else if (strncmp(response, CONT_CMD_BSY,strlen(CONT_CMD_BSY))==0){
          moretime.tv_sec=1;
          moretime.tv_usec=100000;
          write(1, "command rejected - busy", 23);
          select(0, NULL, NULL, NULL, &moretime);
          write(1, "\r                            \r", 30);
        }else{
          write(1, "> ", 2);
          write(1, response, numbytes);
          write(1, "\n", 1);
        }
        numbytes = 0;
      }
      else if (numbytes == 0){
        printf("TUT: connection closed by server\n");
        kill(getppid(), SIGINT);
        break;
      }
    }
  }
  /*
     SENDING LOOP
   */
  else{		// parent process (sends data from command prompt)
    initialize_readline();
    char *s;
    if(read_history(".history.txt") != 0){
      fprintf(stderr, "TUT: failed to load ./.history.txt\n");
    }
    else{
      fprintf(stderr, "loaded ./.history.txt\n");
    }
    while(1){
      buf = readline("");	// fill the buffer with user input
      if (*buf == '\0'){
        command[0] = '\n';
        numbytes = send(sockfd, command, 1, 0);
        if (numbytes == -1)
          perror("send");
      }else if (buf && *buf){
        add_history(buf);			// if there's something in the buffer, add it to history
        //if(strncmp(buf, "help", 4) != 0 && strncmp(buf, "exit", 4) != 0){
        if(strncmp(buf, "exit", 4) != 0){
          i = 0;
          memset(command, '\0', MAXDATASIZE);
          while (*buf != '\0' && i <= MAXDATASIZE){
            command[i] = *buf;
            buf++;
            i++;
          }
          command[i] = *buf;
          numbytes = send(sockfd, command, i+1, 0);
          if (numbytes == -1)
            perror("send");
        }
        if(strncmp(buf, "exit", 4) == 0){
          leave(SIGINT);
        }
        else{
          s = buf;
          execute_line(s);
        }
      }
    }
    leave(SIGINT);
  }
  close(sockfd);
  return 0;
}
