/*
  This is my attempt at writing a client to control the hexapod
*/

#include "ctrl.h"
//#include <arpa/inet.h>

int sockfd;
//int got_status=0;
int acked=0;
char *ip;
int vflag=0;
char max_i[10];
char v_now[10];
unsigned long pkt_cnt=0;

volatile int thread_running=FALSE;
time_t thread_start;
time_t thread_end;

char* parse(char *src, char *dst, char term) {
  while (*src != 0 && *src != term) {
    *dst++=*src++;
  }
  *dst=0;
  return(src);
}


/*
  Command buffer used to communicate between the main program and the server comms thread.
  The first byte of cmdbuff acts as a mutex.
  If null the main program is allowed to write a new command into the buffer.
  If it is not null, the the server comms thread will write to to the server and reset to null.
*/
#define MAXCMD 50           // Max size of a command to send
char cmdbuff[MAXCMD + 1];

void send_cmd(char *cmd) {
  if (strlen(cmd) > MAXCMD) {
    fprintf(stderr,"Command %s is too long. Max length is %d\n", cmd, MAXCMD);
    return;
  }
  while (cmdbuff[0]!=0) sched_yield(); // wait about until we can write a new command

  if (vflag)
    printf("Queueing command %s", cmd);
  strcpy(&cmdbuff[1], cmd+1);
  cmdbuff[0]=*cmd;
}

void thread_cleanup() {
  thread_end=time(NULL);
  thread_running=FALSE;
}

void* thread_server(void* data)
{
  struct sockaddr_in servaddr;
  thread_running=TRUE;
  thread_start=time(NULL);
  
  pthread_cleanup_push(&thread_cleanup, 0);
  
  max_i[0]=0;
  v_now[0]=0;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    fprintf(stderr,"Socket creation failed...%s\n",strerror(errno));
    pthread_exit(&errno);
  } else
    if (vflag)
      printf("Socket successfully created..\n");
  
  bzero(&servaddr, sizeof(servaddr));
 
  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(ip);
  servaddr.sin_port = htons(PORT);
  
  // connect the client socket to server socket
  if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
    fprintf(stderr,"Connection with the server failed...%s\n", strerror(errno));
    pthread_exit(&errno);
  } else
    if (vflag)
      printf("Connected to the server..\n");

  char buff[1024];
  int n;
  int count=0;
  for (;;) {

    n=read(sockfd, buff, sizeof(buff));
    if (n!=0) {
      // print buffer which contains the client contents
      // ready:BPS=  1|V=---|I=---|IP=192.168.1.106|LEGS=------|FLAGS=000000100
      if (n==-1) {
        fprintf(stderr, "Read from server failed %s\n", strerror(errno));
        n=errno; // save the error before we corrupt it
        close(sockfd);
        pthread_exit(&n);
      }
      pkt_cnt++;
      buff[n]=0;
      char bps[5];
      char v[10], i[10], ip[20];
      char *c=parse(buff, status, ':');
      c+=5; c=parse(c, bps, '|');
      c+=3; c=parse(c, v, '|');
      c+=3; c=parse(c, i, '|');
      c+=4; c=parse(c, ip, '|');
      c+=6; c=parse(c, legs, '|');
      c+=7; c=parse(c, flags, 0x0A);

      if (strcmp(i, max_i) > 0)
        strcpy(max_i, i);
      if (STATUS_ON)
        strcpy(v_now,v);

      if (READY) {
        if (count<0) {
          if (vflag)
            printf("Trying to exit thread\n");
          pthread_exit(&errno);
          if (vflag)
            printf("Trying to exit thread again\n");
          break;
        }
        if (count > 0) {
          count--;
          //                 printf("%s Die count %d\n",flags, count--);
          if (count==0) --count;
          write(sockfd, "ack\n", 4);
        } else {
          if (cmdbuff[0] != 0 && acked==1) {
            if (strcmp(cmdbuff, "die\n") == 0) {
              count=150;
              cmdbuff[0]=0;
              write(sockfd, "ack\n", 4);  // try a final ack
            } else {
              write(sockfd, cmdbuff, strlen(cmdbuff));
              if (vflag)
                printf("(%s) Sent to server %s", flags, cmdbuff);
              cmdbuff[0]=0;
              acked=0;
            }
          } else {
            write(sockfd, "ack\n", 4);
            acked=1;
          }
        }
      } else {
        //        printf("(%s) Server status %s\n", flags, status);
        write(sockfd, "ack\n", 4);
        acked=1;
      }
    }
    sched_yield();
    if (count<0) {
      if (vflag)
        printf("%s Trying to exit thread yet again!\n",flags);
      break;
    }
  }
  pthread_cleanup_pop(TRUE);
  pthread_exit(&errno);
}



int main(int argc, char **argv) {
  pthread_t server_id;
  int x;
  int *res=&x;
  {
    char c;
    int help=0;
    struct in_addr sa;
      
    while ((c = getopt (argc, argv, "vh")) != -1) {
      switch (c) {
      case 'v':
        vflag = 1;
        break;
      case 'h':
      default:
        help=1;
      }
    }
  
    ip=argv[argc-1];
    
    if (inet_aton(ip, &sa) != 1) {
      fprintf(stderr, "%s is not a valid i/p address (%s)\n", ip, strerror(errno));
      help=1;
    }
    
    if (help) {
      printf("Usage %s [-vh] <ip addr>\n\t\t-v produces some diagnostics (Verbose mode)\n", argv[0]);
      exit(0);
    }
  }
  // Create thread to read from/write to the server
  x=pthread_create(&server_id, NULL, thread_server, "server");
  if (x!=0) {
    fprintf(stderr, "Thread creation failed %s\n",strerror(errno));
    exit(errno);
  }

  // Wait until we have at least one packet from the server
  // Have some timeout processing in case we never connect to our i/p address
  {
    time_t start_time = time(NULL) + 5;

    while (acked==0) {
      if (sched_yield() != 0) {
        fprintf(stderr,"sched_yield() failed %s\n", strerror(errno));
      }
      if (time(NULL) > start_time) {
        fprintf(stderr,"Timed out waiting for the server\n");
        exit(0);
      }
    }
  }
  // Now try and get into some sort of known state
  // sitting down and powered off
  
  if ( ! STATUS_AUTOSIT) {
    send_cmd("autosit\n");
  }

  if (STATUS_ON) {
    send_cmd("torque\n");
  }

  // No matter what we should now be sitting down and powered off
  char *buffer;
  size_t siz=512;

  buffer = (char *)malloc(siz * sizeof(char));
  while (getline(&buffer,&siz,stdin) !=-1 && thread_running==TRUE) {
    if (buffer[0] == '?') {
      int secs=1;
      sscanf(buffer,"?%d",&secs);
      if (vflag)
        printf("Sleeping for %d secs\n", secs);
      sleep(secs);
    } else {
      if (buffer[0] != '#')
        send_cmd(buffer);
    }
  }

  if (thread_running==TRUE) {
    send_cmd("die\n");
    sched_yield();
  }

  // Waiting for the created thread to terminate
  pthread_join(server_id, (void **) &res);

  if (vflag) {
    printf("This line will be printed after thread ends with %s\n", strerror(*res));
    printf("Max current was %s\n", max_i);
    printf("Battery now %sV\n", v_now);
    float elapsed_time=thread_end - thread_start;
    if (elapsed_time < 1)
      elapsed_time=1.0;
    printf("Server thread running for %ld secs\n", thread_end - thread_start);
    printf("Packets received %ld approx %6.3f/sec\n", pkt_cnt, pkt_cnt/elapsed_time);
  }
  pthread_exit(NULL);
}
