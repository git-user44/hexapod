/*
  This is my attempt at a client to drive the Chica Server
*/
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <errno.h>
#include <pthread.h>
#include <sched.h>

#define PORT 18711

extern char status[10];
extern char legs[7];
extern char flags[9];

char status[10];
char legs[7];
char flags[9];

#define READY          strcmp(status, "ready") == 0

#define STATUS_ON       (flags[0]=='1')
#define STATUS_STANDING (flags[1]=='1')
#define STATUS_AUTOSIT  (flags[6]=='1')

extern void send_cmd(char *);

#define TRUE 1
#define FALSE 0
