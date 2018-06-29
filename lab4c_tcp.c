/* lab4c_tcp.c

* Assignment: CS 111 p4c - Spring 2017
*/

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h> 
#include <math.h> //for log func to calc temperature
#include <poll.h>
#include <sys/types.h> //open
#include <sys/stat.h> //open
#include <fcntl.h> //open
#include <unistd.h> //read/write/fstat/termios/tcgetattr/
#include <termios.h> //termios/tcgetattr
#include <ctype.h> //isdigit

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> //gethostbyname
#include "mraa.h"
/* Summary of exit codes:
	0: successful run
	1: invalid command-line parameters (e.g. unrecognized parameter, no such host)
	2: other run-time failures
*/
const int RET_SUCCESS = 0;
const int RET_FAIL = 1;
const int RET_ERR = 2;
/* accepts the following parameters:

    --id=9-digit-number
    --host=name or address
    --log=filename
    (required) port number

Note: that there is no --port= in front of the port number. This is non-switch parameter.  */

/* valid commands:
	OFF
	STOP
	START
	SCALE=F
	SCALE=C
	PERIOD=seconds
 */
 
 /*
 can assume that the sensors are connected as recommended by the Grove documentation:

    The temperature sensor to Analog input 0.

 */

const int THERM_PIN = 0; 

const int TIMESTR_LEN = 9;
const int BUFFSIZE = 1024;
const char LF_CHAR = 0x0A; //newline
const int B = 4275; //B value for thermistor
const int R0 = 100; //R0 is 100k ohms
const unsigned long long BILLION = 1000000000;

int b_debug = 0; // debug argument
int b_celsius = 0; //celsius if == 1, else Fahrenheheithgt
int b_log = 0; //logging enabled
int b_report = 1; //create reports? controlled with STOP / START commands
int b_button_on = 1; // 1 means on, 0 means power off

// for connection stuff
int portnum = -1;
int idnum = 867530900; //default - must be 9 digits
char* hostname = NULL;

//function declarations
void error(char* msg);
void errorExit(char* msg);
float celsius2F(float tempInC);
int get_formatted_time(char* timeString, size_t size); //return -1 on fail
void sig_handler(int signo);
unsigned long long timeDiff(struct timespec *start_time, struct timespec *end_time);

/* ==================================================================================== */
/* start main() ======================================================================= */
int main(int argc, char** argv){
// default values
int periodSec = 1;
int i; //for indices in main
char timeString[TIMESTR_LEN];
char commandStr[BUFFSIZE];
int comStrLen = 0; //always 1 past the end of the current interesting data

/* start processing options =========================================================== */
static struct option long_options[] = {
	{"id", 		required_argument, 0, 	'i'},
	{"host", 	required_argument, 0, 	'h'},
	{"log", 	required_argument, 0, 	'l'},
	{"period", 	required_argument, 0, 	'p'},
	{"scale", 	required_argument, 0, 	's'},
	{"debug", 	no_argument, 0, 		'd'},
	{0,		0,		0, 	 0}
};

int option_index = 0; //required for getopt
int opt, logfd, n;
//process all options
while((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1){
	
	switch(opt)
	{
	case 'i': /* id */
		if(optarg != NULL){
			//check for proper length (9 digits)
			if(strlen(optarg) != 9){
				fprintf(stderr, "Error: id must be a 9 digit number.\n");
				exit(RET_FAIL);
			}
			
			int tempnum = atoi(optarg);
			if(tempnum > 0){
				idnum = tempnum;
			}
			else{
				/* invalid argument */
				fprintf(stderr, "Error: accepted ID num must be a 9 digit integer.\n");
				exit(RET_FAIL);
			}
		}
		else{
			fprintf(stderr, "Error processing options.\n");
			exit(RET_FAIL);
		}
		break;
	case 'h': /* host */
		if(optarg != NULL){
			//malloc space for hostname/address
			hostname = malloc((strlen(optarg) +1)*sizeof(char));
			if(hostname == NULL)
				errorExit("Error: couldn't malloc hostname.\n");
			
			strcpy(hostname, optarg);
			
		}
		else{
			fprintf(stderr, "Error processing options.\n");
			exit(RET_FAIL);
		}
		break;
	case 'p': /* period */
		if(optarg != NULL){
			int tempnum = atoi(optarg);
			if(tempnum > 0){
				periodSec = tempnum;
			}
			else{
				/* invalid argument */
				fprintf(stderr, "Error: accepted arguments for period must be integers >= 1\n");
				exit(RET_FAIL);
			}
		}
		else{
			fprintf(stderr, "Error processing options.\n");
			exit(RET_FAIL);
		}
		break;
	case 's': /* scale */
		if(optarg != NULL){
			if(strlen(optarg) != 1){
				/* invalid argument */
				fprintf(stderr, "Error: accepted arguments for scale: C or F\n");
				exit(RET_FAIL);
			}
			
			if(*optarg == 'c' || *optarg == 'C'){
				b_celsius = 1;
			}
			else if (*optarg == 'f' || *optarg == 'F'){
				b_celsius = 0;
			}
			else{ /* invalid argument */
				fprintf(stderr, "Error: accepted arguments for scale: C or F\n");
				exit(RET_FAIL);
			}
		}
		else{
			fprintf(stderr, "Error processing options.\n");
			exit(RET_FAIL);
		}
		break;
	case 'l': /* log */
		b_log = 1;	
		logfd = creat(optarg, 0666);
		if(logfd < 0){
			fprintf(stderr, "unable to open log file\n");
			perror(strerror(errno));
			exit(RET_ERR);
		}
		break;
	case 'd': /* debug */
		b_debug = 1;
		break;
	case '?': /*unrecognized argument or no required_argument given*/
		fprintf(stderr, "Error: accepted arguments: id=(9-dig#), host=(name), period=#, scale=C/F, log=filename, debug\n");
		exit(RET_FAIL);
		break;
	default:
		break;
	} //end switch
} //end while getopt

if(optind >= argc){
	fprintf(stderr, "Expected port number after options.\n");
	exit(RET_FAIL);
}

portnum = atoi(argv[optind]);

if(argv[optind+1] != NULL){
	fprintf(stderr, "Too many arguments.\n");
	exit(RET_FAIL);
}

if(hostname == NULL){
	//malloc space for hostname/address
	hostname = malloc((strlen("lever.cs.ucla.edu") +1)*sizeof(char));
	if(hostname == NULL)
		errorExit("Error: couldn't malloc hostname.\n");
	
	strcpy(hostname, "lever.cs.ucla.edu");
}

/* done processing optargs =============================================================== */
if(b_debug){ /* print optargs and test get time and output */
	
	fprintf(stderr, "hostname=%s, id=%d, portnum=%d\n", hostname, idnum, portnum);
	
	if(get_formatted_time(timeString, sizeof(timeString)) == -1){
		fprintf(stderr, "get time error\n");
		exit(RET_ERR);
	}
	fprintf(stderr, "time is: %s\n", timeString);
}

// register sig handler
//if(signal(SIGSEGV, sig_handler) == SIG_ERR)
//	fprintf(stderr, "can't catch SIGSEGV\n");

/* set up temp ========================================================================= */
mraa_aio_context thermPin;
float thermValue; //read into this
float R, temperature; //for calculating temperature

mraa_init();

//inits
thermPin = mraa_aio_init(THERM_PIN);

/* done with set up temp  ============================================================= */

/* start set up socket ==================================================================== */
int sockfd;
struct sockaddr_in serv_addr;
struct hostent *server;

sockfd = socket(AF_INET, SOCK_STREAM, 0);
if(sockfd < 0)
	error("error opening socket");
	
server = gethostbyname(hostname);
if(server == NULL){
	fprintf(stderr, "Error, no such host.\n");
	exit(RET_FAIL);
}

bzero((char*) &serv_addr, sizeof(serv_addr));
serv_addr.sin_family = AF_INET;
memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
serv_addr.sin_port = htons(portnum);

if (connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	error("ERROR connecting");
	
// send id to server
if(dprintf(sockfd, "ID=%d\n", idnum) < 0)
	errorExit("Error writing to socket\n");

/* end set up socket ==================================================================== */
	
struct timespec start_time, end_time;
unsigned long long ns;

/* while button on, poll for input ======================================================= */

//create struct for poll
int ret_poll;
const int numFDS = 1; //how many sources to poll from
struct pollfd fds[numFDS];
fds[0].fd = sockfd; /* from socket */
fds[0].events = POLLIN;

// check for input from stdin
while(b_button_on){
	
	//if making reports, check time. if time, output report 
	if(b_button_on && b_report){
		if(clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
			error("get end time failed\n");

		ns = timeDiff(&start_time, &end_time);
		// if period elapsed, 
		if(ns > (unsigned long long)(periodSec*BILLION)){
			//harvest temperature 
			thermValue = mraa_aio_read(thermPin);
			
			R = 1023.0/((float)thermValue) - 1.0; //promote to float
			R = 100000.0 * R;
			
			temperature = 1.0/(log(R/100000.0)/B+1/298.15) - 273.15;
			
			// create report
			if(b_celsius == 0){
				temperature = celsius2F(temperature);
			}
			//get time (formatted)
			if(get_formatted_time(timeString, TIMESTR_LEN) != 0){
				exit(RET_ERR);
			}
			
			//update timer
			if(clock_gettime(CLOCK_MONOTONIC, &start_time) < 0)
				error("get start time failed\n");
			
			dprintf(sockfd, "%s %04.1f\n", timeString, temperature);
			
			if(b_log)
				dprintf(logfd, "%s %04.1f\n", timeString, temperature);
			

		}
		
	} /* done reporting temperature */
	
/* poll ============================================================================= */
	ret_poll = poll(fds, numFDS, 0);
	if(ret_poll == -1){
		perror("poll error");
		exit(RET_ERR);
	}
	
	if(ret_poll > 0){ /* check for pending input or errors */
		if(b_debug){
			fprintf(stderr, "polling input from kb\n");
		}
		if(fds[0].revents & POLLIN){ /* input from keyboard */
			int bytesRead = 0;
			int b_continue = 1;
			char buffer[BUFFSIZE];
			//try to read input
			if((bytesRead = read(fds[0].fd, buffer, BUFFSIZE)) > 0){
				if(b_debug){
					fprintf(stderr, "stuff to read\n");
				}
				for(i = 0; i < bytesRead; i++){
					//prevent overflow
					if(comStrLen >= (BUFFSIZE-1)){
						fprintf(stderr, "nobody needs that long a command\n");
						exit(RET_ERR);
					}
					//process byte
					int c = buffer[i];
					if(b_debug){
						fprintf(stderr, "char entered: %c\n", (char)c);
					}
					switch(c){
					case 0x0A: /* <lf> newline */
						//end of command, append null char
						commandStr[comStrLen] = '\0';
						comStrLen++;
						//b_continue = 0;
						/* check for finished command string */
						if(commandStr[comStrLen-1] == '\0'){
							
							//compare with valid commands
							if(strncmp(commandStr, "OFF", strlen("OFF")) == 0){
								b_button_on = 0;
							}
							else if(strncmp(commandStr, "STOP", strlen("STOP")) == 0){
								b_report = 0;
							}
							else if(strncmp(commandStr, "START", strlen("START")) == 0){
								//if already on, just log command. else turn on and take starttime
								b_report = 1;			
							}
							else if(strncmp(commandStr, "SCALE=F", strlen("SCALE=F")) == 0){
								b_celsius = 0; //celsius if == 1, Fahrenheheithgt if == 0
							}
							else if(strncmp(commandStr, "SCALE=C", strlen("SCALE=C")) == 0){
								b_celsius = 1; //celsius if == 1, Fahrenheheithgt if == 0
							}
							else if(strncmp(commandStr, "PERIOD=", strlen("PERIOD=")) == 0){
								int b_valid = 1;
								int tempPer = 0;
								//if there is a period=(Something)
								if(strlen(commandStr) > strlen("PERIOD=")){
									//check each char to ensure all numeric starting after '='
									int j;
									for(j = 7; (commandStr[j] != '\0') && b_valid; j++){
										// check to end. if not digit, invalid. 
										if(isdigit(commandStr[j]) == 0){
											b_valid = 0;
										}
									}
								}
								
								if(b_valid){
									tempPer = atoi(&commandStr[7]);
									if(tempPer >= 1){
										periodSec = tempPer;
									}
									else
										b_valid = 0;
								}
								
								//confirm valid entry
								if(b_valid == 0){
									fprintf(stderr, "valid command format: PERIOD=# (integer)seconds.\n");
									exit(RET_ERR);
								}
							}
							else{ /* invalid command */
								//log and exit
								if(b_log){
									dprintf(logfd, "INVALID COMMAND\n");
								}
								fprintf(stderr, "valid commands: OFF, STOP, START, SCALE=F/C, PERIOD=secs.\n");
								exit(RET_ERR);
							}
							
							//log receipt of command
							if(b_log)
								dprintf(logfd, "%s\n", commandStr);
							
							if(b_debug){
								fprintf(stderr, "command entered: %s. periodSec=%d.\n", commandStr, periodSec);
							}
							
							//command has been processed, reset command string
							comStrLen = 0;			
						} //done checking for finished string command
					break;
					default:
						commandStr[comStrLen] = (char)c;
						comStrLen++;
					break;
					} //end switch
					if(b_debug){
						fprintf(stderr, "char appended: %c\n", commandStr[comStrLen-1]);
					}
				} //end for bytesRead			
			} //end if read > 0
		} //done processing input from stdin
		if(fds[0].revents & POLLERR || (fds[1].revents & POLLHUP)){
			//get final time
			if(get_formatted_time(timeString, TIMESTR_LEN) != 0){
				exit(RET_ERR);
			}
			
			dprintf(sockfd, "%s SHUTDOWN\n", timeString);
			
			if(b_log){
				dprintf(logfd, "%s SHUTDOWN\n", timeString);
			}
			exit(RET_SUCCESS);
		}

		
	} //done with a poll and commands
	


}

/* initiate shutdown ====================================================================== */

// send final report
//get final time
if(get_formatted_time(timeString, TIMESTR_LEN) != 0){
	exit(RET_ERR);
}

dprintf(sockfd, "%s SHUTDOWN\n", timeString);

if(b_log){
	dprintf(logfd, "%s SHUTDOWN\n", timeString);
	close(logfd);
}
/* shutdown message has been sent =========================================================== */

mraa_result_t ret = MRAA_SUCCESS;
ret = mraa_aio_close(thermPin);
if(ret != MRAA_SUCCESS){
	mraa_result_print(ret);
	fprintf(stderr, "mraa_aio_close thermPin failed.\n");
	exit(RET_ERR);
}

exit(RET_SUCCESS);

} /* end main ========================================================================== */

/* get_formatted_time returns 0 if success, -1 if failure */
int get_formatted_time(char* timeString, size_t size){
	time_t current_time;
	struct tm* time_info;

	// space for HH:MM:SS\0
	if(size < TIMESTR_LEN){
		fprintf(stderr, "not enough space to store time string.\n");
		return -1;
	}
	else{
		current_time = time(NULL);
		if(current_time == (time_t)(-1)){
			fprintf(stderr, "localtime error\n");	
			return -1;
		}
		time_info = localtime(&current_time);
		if(time_info == NULL){
			fprintf(stderr, "timeinfo error\n");
		}
		
		if(strftime(timeString, size, "%H:%M:%S", time_info) == 0){
			fprintf(stderr, "strftime returned 0\n");
			return -1;
		}
	}	
	return 0;
}

unsigned long long timeDiff(struct timespec *start_time, struct timespec *end_time){
	unsigned long long ns;
	ns = end_time->tv_sec - start_time->tv_sec;
	ns *= 1000000000; //convert seconds to nanoseconds
	ns += end_time->tv_nsec;
	ns -= start_time->tv_nsec;	
	return ns;
}

float celsius2F(float tempInC){
	
	return ((tempInC * 1.8) + 32);
}

void sig_handler(int signo){
	if (signo == SIGSEGV){
		if(b_debug)
			fprintf(stderr, "received SIGSEGV\n");
	}

}

void error(char* msg){
	perror(msg);
	exit(RET_ERR);
}

void errorExit(char* msg){
	fprintf(stderr, "%s", msg);
	exit(RET_ERR);
}