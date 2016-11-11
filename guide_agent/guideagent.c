
/*
 * guideagent.c - program to display data on codedrops and field agents and send hints to field agent
 *
 * Usage: ./guideagent [-log=raw] teamName playerName GShost GSport
 *
 * Jiyun Sung, May 2016
 * GTK code written by Saisi provided by CS50 class
 */
// functions in c 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <strings.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

// gtk functions
#include <cairo.h>
#include <gtk/gtk.h>
#include <math.h>

// local library
#include "../common/common.h"
#include "../lib/cs50ds.h"

/***************** global types *******************/
typedef struct fieldAgent { // struct to store field agent data
  char *pebbleId;
  char *playerName;
  char *teamName;
  char *status;
  char *lati;
  char *longi;
  char *lastChecked;
  cairo_t* cr;
  cairo_surface_t* image;
} fieldAgent;

typedef struct codeDrop { // strct to store code drop data
  char *codeId;
  char *lati;
  char *longi;
  char *neuTeam;
  cairo_t* cr;
  cairo_surface_t* image;
} codeDrop;


/************* Global variables ***********/
      
// socket related
int socketGA;
char *hostname;
int port;
struct sockaddr_in server;  // address of the server

// Initialize Game
FILE *logfile;
bool logRaw = false;

char *statusReq = "1"; // whether or not guide agent wants a status update from game server
char *guideId;
char *gameId; // to initialize with the first GA_STATUS update, gameId is 0. After game server sends its first update, it will become that.
char *teamName;
char *playerName;

bool gameOn = true;
bool printedPebbleId = false; // upon the first receipt of GAME_STATUS message and list of field agents, print out their pebble Ids and Names to facilitate function of Guide Agent

// number of field agents and code drops - pass to GTK functions
int falength = 0;
int cdlength = 0;
int neutcd = 0;

// array of struct fieldAgent and codeDrop to store the data. Initialize the first subfield as NULL.
struct fieldAgent fieldAgents[10];
struct codeDrop codeDrops[50];



/************* local functions ****************/
static bool GAME_STATUS(char **message, struct sockaddr_in *them);
static bool GA_STATUS(int socket, struct sockaddr_in *them);
static bool GA_HINT(int socket, struct sockaddr_in *them, char *response);
static bool GAME_OVER(char **message, struct sockaddr_in *them);
static bool parseFieldAgent(char *message);
static bool parseCodeDrop(char *message);
static bool parseTeam(char *message);  
static void delete_parsed(char **parsed, int length);
static void delete_fieldAgents(int length);
static void delete_codeDrops(int length);
static char* makeHexdigits(int strLen);

/*************** gtk functions ****************/
void initialize_window(GtkWidget* window);
static void do_drawing(cairo_t *);

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static void do_drawing(cairo_t *cr);
static void draw_codedrops(cairo_t *cr);
static void draw_fieldagents(cairo_t *cr);

static void custom_background(cairo_t* cr);
static gboolean time_handler(GtkWidget *widget);

int start = 0;
int HEIGHT=600;
int WIDTH=1012;
double x_1 = -72.296950;
double y_1 = 43.709183;
double rwidth = -72.283777 + 72.296950;
double rheight = 43.702618 - 43.709183;
int cdindex;
int faindex;


cairo_surface_t* pic = NULL;
cairo_pattern_t* pat = NULL;


int main(const int argc, char *argv[])
{
  int i = 1;

  if (argc != 5 && argc != 6) {
    printf("Incorrect number of arguments!\n");
    exit(1);
  }
  if (strcmp(argv[i], "-log=raw") == 0) {
    logRaw = true;
    i++;
  }

  teamName = argv[i];
  i++;
  playerName = argv[i];
  i++;
  hostname = argv[i];
  port = atoi(argv[i+1]);

  // Look up the hostname specified on command line
  struct hostent *hostp = gethostbyname(hostname);
  if (hostp == NULL) {
    fprintf(stderr, "%s: unknown host \n", hostname);
    exit(3);
  }
  
  // Initialize fields of the server address

  server.sin_family = AF_INET;
  bcopy(hostp->h_addr_list[0], &server.sin_addr, hostp->h_length);
  server.sin_port = htons(port);

  // create socket
  socketGA = socket(AF_INET, SOCK_DGRAM, 0);
  if (socketGA < 0) {
    fprintf(stderr, "Failed to creat socket.\n");
    exit(2);
  }
  
  // initialize game Id as 0
  gameId = malloc(10 * sizeof(char));
  strcpy(gameId, "0");

  // intialize random guide Id
  guideId = makeHexdigits(6);
  
  //start writing log file 
  if ((logfile = fopen("log/log.txt", "a")) == NULL) {
    fprintf(stderr, "Unable to open log file\n");
    exit(4);
  }

  fprintf(logfile, "Game Start!\n");
  
  fprintf(logfile, "Host: %s\n", hostname);
  fprintf(logfile, "Port: %d\n", port);

  // first status update
  GA_STATUS(socketGA, &server);
  fprintf(stdout, "Guide agent logging in...");
  
  //gtk_init(&argc, &argv);
  int dummyc=1;
  gtk_init(&dummyc, &argv);

  GtkWidget *window;
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  /* IMPORTANT, THIS IS THE WIDGET WE DRAW ON */
  GtkWidget *darea = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(window), darea);
  
  /* ENSURE YOU CAPTURE THE draw signal on the Drawable Area! */
  g_signal_connect(G_OBJECT(darea), "draw",G_CALLBACK(on_draw_event), NULL);
  g_signal_connect(window, "destroy",G_CALLBACK(gtk_main_quit), NULL);
  
  initialize_window(window);
  
  /*KEY ANIMATION LINE
    animate every 100 milliseconds
    time_handler is BOILER PLATE FUNCTION
  */
  g_timeout_add(100, (GSourceFunc) time_handler, (gpointer) window);
  
  gtk_main();
  
  /* always destroy surface once you are done*/
  cairo_surface_destroy(pic);
  cairo_pattern_destroy(pat);
  
  return 0; //success  
}

static int game_main(cairo_t *cr)
{
    
  // read from either the socket or stdin, whichever is ready first;
  // if stdin, read a line and send it to the socket;
  // if socket, receive message from socket and write to stdout. 

  // for use with select()
  fd_set rfds;              // set of file descriptors we want to read
  struct timeval timeout;   // how long we're willing to wait
  const struct timeval fivesec = {5,0};   // five seconds
  
  // Watch stdin (fd 0) and the UDP socket to see when either has input.
  FD_ZERO(&rfds);
  FD_SET(0, &rfds);         // stdin
  FD_SET(socketGA, &rfds); // the UDP socket
  int nfds = socketGA+1;   // highest-numbered fd in rfds
  
  // Wait for input on either source, up to five seconds.
  timeout = fivesec;
  int select_response = select(nfds, &rfds, NULL, NULL, &timeout);
  // note: 'rfds' updated, and value of 'timeout' is now undefined
  
  if (select_response < 0) {
    // some error occurred
    perror("select()");
    exit(9);
  } else if (select_response == 0) {
    // timeout occurred; status update
    GA_STATUS(socketGA, &server); // send a status update to the server
    return 0;
  } else if (select_response > 0) {
    // some data is ready on either source, or both
    
      // handle standard input
      if (FD_ISSET(0, &rfds)) {
	char *response = readline(stdin);
	if (response == NULL) {// if EOF on stdin, exit
	  free(response);
	  gameOn = false;
	  return 0;
	}
	// send a hint
	GA_HINT(socketGA, &server, response);

	free(response);
	
      }
      // handle socket input
      if (FD_ISSET(socketGA, &rfds)) {
	char *message;
	
	int nbytes = readMessage(socketGA, &message, &server); // dynamic allocation
	if (nbytes < 0) {
	  fprintf(stderr, "Invalid message\n");
	  free(message);
	  return 0;
	} else {
	  printf("From %s, port %d: '%s\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port), message);

	  if (strlen(message) == 0) { // empty message
	    free(message);
	    return 0;
	  }

	  char parsingMessage[strlen(message+1)];
	  char **parsedMessage = parseMessage(strncpy(parsingMessage, message, strlen(message)), strlen(message), 1); // returns a double  character array with each subfield parsed by "|" as separate arrays

	  if (parsedMessage == NULL) { // wrong opcode
	    fprintf(logfile, "Opcode Invalid!\n");
	    free(message);
	    return 0;
	  }

	  if (strcmp(parsedMessage[0], "GAME_OVER") == 0) {
	    if (GAME_OVER(parsedMessage, &server));
	    gameOn = false; // exit loop
	    delete_parsed(parsedMessage, 4); // four subfields in GAME_OVER
	    free(message);
	    return 0;
	  }

	  if (strcmp(parsedMessage[0], "GAME_STATUS") == 0) {

	    //before updating the field agent and code drops information, free the array first
	     delete_fieldAgents(falength);
	     delete_codeDrops(cdlength);
	    
	    if (GAME_STATUS(parsedMessage, &server) && (!printedPebbleId)) { // upon successful parsing of game status message, check if pebble Id's were printed
	      printedPebbleId = true;
	      // print the list of field agents, the first time only
	      fprintf(stdout, "Type the pebble Id of the field agent you want to send a message to and press enter. If you want to send a message to all field agents in the team, just type * and press enter. \n");
	      fprintf(stdout, "List of field agents:\n");

	      fprintf(logfile, "Type the pebble Id of the field agent you want to send a message to and press enter. If you want to send a message to all field agents in the team, just type * and press enter. \n");
	      fprintf(logfile, "List of field agents:\n");

	      
	      // iterate through array of fieldAgents and print the pebble IDs
	      int i = 0;
	      while (fieldAgents[i].pebbleId != NULL) {
		if (strcmp(fieldAgents[i].teamName, teamName) == 0) { // only print the names of field agents in the guide agent's team
		  fprintf(stdout, "field agent %s, pebble ID %s, latitude %s\n", fieldAgents[i].playerName, fieldAgents[i].pebbleId, fieldAgents[i].lati);
		  fprintf(logfile, "field agent %s, pebble ID %s, latitude %s\n", fieldAgents[i].playerName, fieldAgents[i].pebbleId, fieldAgents[i].lati);
		}
		i++;
	      }
	    }
	    delete_parsed(parsedMessage, 4); // GAME_STATUS has 4 subfields
	    free(message); // free memory
	    return 0; // exit normally
	  }

	}
      }
      // print a fresh prompt
      fprintf(stdin, "Type Pebble ID to send a hint: \n");
  }

  return 0; //success
}

//This function handles the `GAME_STATUS` opcode.
static bool GAME_STATUS(char **message, struct sockaddr_in *them)
{  
  char *gameIdtemp = message[1];

  if (strcmp(gameId, "0") == 0) { // first game status update
    strcpy(gameId, gameIdtemp); // gameId is dynamically alloated. copy the received game Id from game server
    fprintf(stdout, "Connected.\n");
    fprintf(stdout, "Game ID = %s\n", gameId);
    fprintf(logfile, "Connected.\n");
    fprintf(logfile, "Game ID = %s\n", gameId);
  } else {
    if (strcmp(gameId, gameIdtemp) != 0) {
      fprintf(stderr, "Game ID invalid!\n");
      return false; // fail
    } else {
      fprintf(logfile, "New status update from game server:\n");
    }
  }
  
  if (!parseFieldAgent(message[2]) || !parseCodeDrop(message[3]))
    return false;

  fprintf(stdout, "Number of field agents: %d, number of remaining code drops: %d\n", falength, cdlength-neutcd);
  fprintf(logfile, "Number of field agents: %d, number of remaining code drops: %d\n", falength, cdlength-neutcd);
  return true; // successfully received game status update
}

//This function sends the `GA_STATUS` opcode.
static bool GA_STATUS(int socket, struct sockaddr_in *them)
{
  char *update = malloc(60);
  strcpy(update, "GA_STATUS|");
  strcat(update, gameId);
  strcat(update, "|");
  strcat(update, guideId);
  strcat(update, "|");
  strcat(update, teamName);
  strcat(update, "|");
  strcat(update, playerName);
  strcat(update, "|");
  strcat(update, statusReq);
  
  // send the message, check if sent properly
  int bytes_sent = sendMessage(socket, update, them);
  if (bytes_sent < 0) {
    fprintf(stderr, "Failed to send!\n");
    if (logRaw == true)
      fprintf(logfile, "Failed to send!\n");
  }
  if (logRaw == true)
    fprintf(logfile, "Guide Agent Status Update: %s\n", update);
  free(update);
  return true;
}

//This function sends the `GA_HINT` opcode.
static bool GA_HINT(int socket, struct sockaddr_in *them, char *response)
{
  char *pebbleId = "empty"; // check the pebble Id
  
  // if user wants to send a hint to all field agents
  if (strcmp(response, "*") == 0) {
    pebbleId = "*";
  } else {
    // index to iterate through array of fieldAgents
    int i = 0;
    while (fieldAgents[i].pebbleId != NULL) {
      if (strcmp(fieldAgents[i].pebbleId, response) == 0) { // check if the user input is correct pebble Id of a field agent in the team
	pebbleId = response;
	break;
      }
      i++;
    }
  }
  if (strcmp(pebbleId, "empty") == 0) { //if failed to initialize pebble ID print an error message
    if (logRaw == true)
      fprintf(logfile, "Invalid pebble ID!\n");
    return false; // fail
  }
  
  char *update = malloc(200);
  strcpy(update, "GA_HINT");
  strcat(update, "|");
  strcat(update, gameId);
  strcat(update, "|");
  strcat(update, guideId);
  strcat(update, "|");
  strcat(update, teamName);
  strcat(update, "|");
  strcat(update, playerName);
  strcat(update, "|");
  strcat(update, pebbleId);
  
  char *hint;
  fprintf(stdout, "Type the hint message you want to send:\n");
  hint = readline(stdin);
  if (strlen(hint) > 140) {
    fprintf(stderr, "Hint too long!\n");
    free(hint);
    free(pebbleId);
    free(update);
    return false;
  }
  strcat(update, "|");
  strcat(update, hint);
  printf("%s\n", update);
  
  int bytes_sent = sendMessage(socket, update, them);
  if (bytes_sent < 0) {
    fprintf(stderr, "Failed to send!\n");
    if (logRaw)
      fprintf(logfile, "Failed to send!\n");
    free(hint);
    free(update);
    free(pebbleId);
    return false;
  }

  fprintf(logfile, "Sent hint to field agent %s. Message: %s\n", pebbleId, hint);
  free(hint);
  free(update);
  return true;
}

// This function reads `GAME_OVER` opcode.
static bool GAME_OVER(char **message, struct sockaddr_in *them)
{
  if (strcmp(gameId, message[1]) != 0) {
    fprintf(stderr, "Wrong gameId!\n");
    return false;
  }
  fprintf(logfile, "GAME OVER\n");
  fprintf(stdout, "GAME OVER\n");
  
  int numcodedrops = sscanf(message[2], "%d", &numcodedrops);  
  fprintf(logfile, "Number of remaining code drops: %d\n", numcodedrops);
  parseTeam(message[3]);

  delete_fieldAgents(falength);
  delete_codeDrops(cdlength);
  
  if (logfile == NULL)
   fprintf(stdin, "logfile NULL!\n");
  
  //game over
  fclose(logfile);
  free(gameId);
  return true;
}

static bool parseFieldAgent(char *message)
{
  // each subfield will represent the data of one FA - pebbleId, teamName, playerName, playerStatus, lastKnownLat, lastKnownLong, secondsSinceLastContact
  char **fastr = (char**) calloc(10, sizeof(char*)); // field agent string, 10 field agents at most; dynamically allocate it so that it won't be affected by strtok
  int index = 0;
  char *fa;

  if (strcmp(message, "NULL") == 0) { // if game server tells me there are no field agents
    fprintf(stdout, "No field agents!\n");
    fprintf(logfile, "No field agents!\n");
    return false;
  }

  fa = strtok(message, ":"); // temporary char pointer to parse the message
  fastr[index] = (char*) calloc(strlen(fa) + 1, sizeof(char)); // each field agent data
  strncpy(fastr[index], fa, strlen(fa)); // copy the string into the allocated array

  while (index < 10 && (fa = strtok(NULL, ":")) != NULL) {
    index++;
    fastr[index] = (char*) calloc(strlen(fa) + 1, sizeof(char)); // assign memory
    strncpy(fastr[index], fa, strlen(fa));
  }

  if (falength < index) {
    fprintf(stdout, "%d new players added\n", index-falength);
    fprintf(logfile, "%d new players added\n", index-falength);
  } else if (falength > index) {
    fprintf(stdout, "%d players captured\n", falength - index);
    fprintf(logfile, "%d players captured\n", falength - index);
  } 
  falength = index + 1; // the current index is the number of field agents - store it to global variable falength

  index = 0;
  char *subfield;
  
  while (index < 50 && fastr[index] != NULL) {
    
    // fill in each subfield of Field Agent into the struct fieldAgents[]
    if ((subfield = strtok(fastr[index], ",")) != NULL) {
      fieldAgents[index].pebbleId = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].pebbleId, subfield);
    }
    else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      fieldAgents[index].teamName = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].teamName, subfield);
    } else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      fieldAgents[index].playerName = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].playerName, subfield);
    } else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      fieldAgents[index].status = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].status, subfield);
    } else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      fieldAgents[index].lati = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].lati, subfield);
    } else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      fieldAgents[index].longi = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].longi, subfield);
    } else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      fieldAgents[index].lastChecked = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(fieldAgents[index].lastChecked, subfield);
    } else {
      fprintf(stderr, "Field Agent not formatted correctly!\n");
      return false;
    }
    // increment index
    index++;
  }
  // reset index
  index = 0;
  // free memory
  while (index < falength) {
    if (logRaw)
      fprintf(logfile, "field agents %s\n",fieldAgents[index].playerName); 
    free(fastr[index]);
    index++;
  }
  free(fastr);
  return true;
}

static bool parseCodeDrop(char *message)
{
  // each subfield will represent the data of one codeDrop - codeId, lat, long, neutralizingTeam

  char **cdstr = (char**) calloc(50, sizeof(char*)); // 50 code drops at most
  int index = 0;
  char *cd;
  int neutcdtemp = 0; // index to count neutralized code drops

  cd = strtok(message, ":"); // char pointer to temporarily store stripped tokens
  cdstr[index] = (char*) calloc(strlen(cd) + 1, sizeof(char)); // one code drop 
  strncpy(cdstr[index], cd, strlen(cd));

  while (index < 50 && (cd = strtok(NULL, ":")) != NULL) {
    index++;
    cdstr[index] = (char*) calloc(strlen(cd) + 1, sizeof(char)); // assign memory
    strncpy(cdstr[index], cd, strlen(cd)); // copy the string and save it
  }
  if (cdlength == 0 && cdlength != index) {
    fprintf(logfile, "Number of code drops changed!\n");
  }
  cdlength = index + 1; // number of code drops stored, for later usage in free() - global variable
  
  // set the index back to zero
  index = 0;
  char *subfield;
  
  while (cdstr[index] != NULL) {

    // fill in each subfield of code drop into the struct codeDrop
    if ((subfield = strtok(cdstr[index], ",")) != NULL){
      codeDrops[index].codeId = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(codeDrops[index].codeId, subfield);
    } else {
      fprintf(stderr, "Code Drop not formatted correctly!\n");
      return false;
    }
    
    if ((subfield = strtok(NULL, ",")) != NULL) {
      codeDrops[index].lati = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(codeDrops[index].lati, subfield);
    } else {
      fprintf(stderr, "Code Drop not formatted correctly!\n");
      return false;
    }
    
    if ((subfield = strtok(NULL, ",")) != NULL) {
      codeDrops[index].longi = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(codeDrops[index].longi, subfield);
    } else {
      fprintf(stderr, "Code Drop not formatted correctly!\n");
      return false;
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      if (strcmp(subfield, "NONE") != 0) // code drop neutralized
	neutcdtemp++;
      codeDrops[index].neuTeam = (char *) calloc(strlen(subfield) + 1, sizeof(char));
      strcpy(codeDrops[index].neuTeam, subfield);
    } else {
      fprintf(stderr, "Code Drop not formatted correctly!\n");
      return false;
    }
    // increment index
    index++;
  }
  if (neutcd < neutcdtemp) { // more neutralized code drops
    fprintf(stdout, "%d more neutralized code drops\n", neutcdtemp-neutcd);
    fprintf(logfile, "%d more neutralized code drops\n", neutcdtemp-neutcd);
  } else if (neutcd > neutcdtemp) { // less neutrlized code drops ? error!
    fprintf(stderr, "code drop number incorrect\n");
  }
  neutcd = neutcdtemp;
  // reset index
  index = 0;
  // free memory
  while (index < cdlength) {
    free(cdstr[index]);
    index++;
  }
  free(cdstr);
  return true;
}

static bool parseTeam(char *message)
{
  // each subfield will represent the data of one team - teamName numPlayers numCaptures numCaptured numNeutralized
  char **trstr = (char**) calloc(20, sizeof(char*)); // team record string, 20 teams at most
  int index = 0;
  char *tr;
  int length;

  tr = strtok(message, ":");
  trstr[index] = (char*) calloc(strlen(tr) + 1, sizeof(char));
  strncpy(trstr[index], tr, strlen(tr));
  
  while (index < 19 && (tr = strtok(NULL, ":")) != NULL) {
    index++;
    trstr[index] = (char*) calloc(strlen(tr) + 1, sizeof(char)); // allocate memory
    strncpy(trstr[index], tr, strlen(tr));
  }
  length = index; // number of team records stored
  
  // set the index back to zero
  index = 0;
  char *subfield;

  while (trstr[index] != NULL) {

    // each subfield represents some data that should be recorded 
    if ((subfield = strtok(trstr[index], ",")) != NULL)
      fprintf(logfile, "Team Name: %s\n", subfield);
    if ((subfield = strtok(NULL, ",")) != NULL) {
      int numplayer =sscanf(subfield, "%d", &numplayer);
      fprintf(logfile, "number of players: %d\n", numplayer);  
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      int numcapture =sscanf(subfield, "%d", &numcapture);
      fprintf(logfile, "number of Captures: %d\n", numcapture);
    }
    if ((subfield = strtok(NULL, ",")) != NULL) {
      int numcaptured =sscanf(subfield, "%d", &numcaptured);
      fprintf(logfile, "number of captured players: %d\n", numcaptured);
    } 
    if ((subfield = strtok(NULL, ",")) != NULL) {
      int numneutcd =sscanf(subfield, "%d", &numneutcd);
      fprintf(logfile, "number of neutralized code drops: %d\n", numneutcd);
    }
    // increment index
    index++;
  }
  // reset index
  index = 0;
  // free memory
  while (index <= length) {
    free(trstr[index]);
    index++;
  }
  free(trstr);
  
  return true;
}

// delete the parsed message - the double character array returned by parser
// assume the caller knows the length of the array - given by what command it is 
static void delete_parsed(char **parsed, int length) {
  for (int i = 0; i < length; i++) {
    free(parsed[i]);
  }
  free(parsed);
}

// iterate through field agents and free the memory used
static void delete_fieldAgents(int length){
  for (int j = 0; j < length; j++) {
    free(fieldAgents[j].pebbleId);
    free(fieldAgents[j].playerName);
    free(fieldAgents[j].teamName);
    free(fieldAgents[j].lati);
    free(fieldAgents[j].longi);
    free(fieldAgents[j].status);
    free(fieldAgents[j].lastChecked);
  }
}

// iterate through field agents and free the memory used
static void delete_codeDrops(int length) {
  for (int j = 0; j < length; j++) {
    free(codeDrops[j].codeId);
    free(codeDrops[j].lati);
    free(codeDrops[j].longi);
    free(codeDrops[j].neuTeam);
  }
}

static char* makeHexdigits(int strLen) {
	char *str = malloc(strLen + 1);
	for(int i = 0; i < strLen; i++) {
		sprintf(str + i, "%x", rand() % 16);
	}
	return str;
}

/***********************************************************/
/***************** GTK function definitions ****************/
/***********************************************************/

void initialize_window(GtkWidget* window){
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(window), 1012, 811);
    gtk_window_set_title(GTK_WINDOW(window), "GTK window");
    gtk_widget_show_all(window);
}

static void custom_background(cairo_t* cr){
    
    /* CREATE SURFACE FROM IMAGE */
    if(!pic)pic =cairo_image_surface_create_from_png("a.png");
    /* SET SURFACE TO OUR CURRENT WINDOW CONTEXT CR*/
    /* 0,0 IS THE LOCATION */
    cairo_set_source_surface(cr, pic, 0, 0);
    /* NEVER FORGET OT PAINT */
    cairo_paint(cr);
    
}

/* WHY ON EARTH AM I SHOUTING */

static void write_text_custom(cairo_t* cr, char* s, int x, int y, float size){
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    
    /* CREATE A PATTERN ONLY ONCE!! AVOID LEAKS*/
    if(!pat)pat=cairo_pattern_create_linear(0, 15, 0, 90*0.8);
    
    cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
    cairo_pattern_add_color_stop_rgb(pat, 0.0, 1, 0.6, 0);
    cairo_pattern_add_color_stop_rgb(pat, 0.5, 1, 0.3, 0);
    
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);;
    cairo_text_path(cr, s);

    cairo_set_source(cr, pat);
    cairo_fill(cr);
}


/*
 CALLED WHEN DRAW EVENT SIGNAL IS RELEASED
 */
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  game_main(cr); // update game 
  do_drawing(cr); // draw code drops and field agents on the map
  if (!gameOn) // if game over received
    gtk_main_quit(); // quit the main loop
  return FALSE;
}

//BOILER PLATE CALLBACK
static gboolean time_handler(GtkWidget *widget)
{
    gtk_widget_queue_draw(widget);
    return TRUE;
}

static void do_drawing(cairo_t *cr)
{
  custom_background(cr); // call the map

  cdindex = 0; // index to loop over all the code drops
  while (cdindex < cdlength) { 
    draw_codedrops(cr); // call the draw function for each 
    cdindex++;
  }
    
  faindex = 0; // index to loop over all the field agents
  while (faindex < falength) {
    draw_fieldagents(cr); // call the draw function for each field agent
    faindex++;
  }
  
}

// draw the field agents as mint colored squares with their pebble ID
static void draw_fieldagents(cairo_t *cr){
  
  fieldAgents[faindex].cr = cr;
  cairo_set_line_width(cr, 3);
  cairo_set_source_rgb(cr, 0, 1, 1);
  cairo_fill(cr);
  
  
  double latitude = (atof(fieldAgents[faindex].lati)-y_1)/rheight*HEIGHT;
  double longitude = (atof(fieldAgents[faindex].longi)-x_1)/rwidth*WIDTH;
 
  write_text_custom(fieldAgents[faindex].cr, fieldAgents[faindex].pebbleId, longitude, latitude, 12.0); // write their pebble Id on top
  cairo_rectangle(cr, longitude, latitude, 14, 14);  // draw the field agents
}

// draw the code drops as red squares, if they're neutralized, make it brown
static void draw_codedrops(cairo_t *cr){
  
  codeDrops[cdindex].cr = cr;
  cairo_set_line_width(cr, 3);
  if (strcmp(codeDrops[cdindex].neuTeam, "NONE") == 0)
    cairo_set_source_rgb(cr, 1, 0, 0);
  else
    cairo_set_source_rgb(cr, 0.5, 0.2, 0.3);
  cairo_fill(cr);

  // scale the coordinates to match the map size
  double latitude = (atof(codeDrops[cdindex].lati)-y_1)/rheight*HEIGHT;
  double longitude = (atof(codeDrops[cdindex].longi)-x_1)/rwidth*WIDTH;

  cairo_rectangle(cr, longitude, latitude, 10, 10);
}
