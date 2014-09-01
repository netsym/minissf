#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void syserr(const char* msg) { perror(msg); exit(-1); }

int main(int argc, char* argv[])
{
  int sockfd, portno, rate, npings;
  struct hostent* server;
  struct sockaddr_in serv_addr;
  char buffer[256];

  if(argc != 5) {
    fprintf(stderr, "Usage: %s <hostname> <port> <pings_per_second> <num_pings>\n", argv[0]);
    return 1;
  }
  server = gethostbyname(argv[1]);
  if(!server) {
    fprintf(stderr, "ERROR: no such host: %s\n", argv[1]);
    return 2;
  }
  portno = atoi(argv[2]);

  rate = atoi(argv[3]);
  if(rate <= 0) {
    fprintf(stderr, "ERROR: invalid number of pings per second: %s\n", argv[3]);
    return 3; 
  }
  useconds_t sleeptime = 100000/rate;
  
  npings = atoi(argv[4]);
  if(npings <= 0) {
    fprintf(stderr, "ERROR: invalid num_pings: %s\n", argv[4]);
    return 4; 
  }

  /*{
  struct in_addr **addr_list; int i;
  printf("Official name is: %s\n", server->h_name);
  printf("    IP addresses: ");
  addr_list = (struct in_addr **)server->h_addr_list;
  for(i = 0; addr_list[i] != NULL; i++) {
    printf("%s ", inet_ntoa(*addr_list[i]));
  }
  printf("\n");
  }*/

  sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(sockfd < 0) syserr("can't open socket");
  printf("create socket...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  serv_addr.sin_port = htons(portno);

  if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    syserr("can't connect to server");
  printf("connect...\n");

  for(int i=0; i<npings; i++) {
    if(i && (i%rate) == 0) printf("sent %d\n", i);
    int n = send(sockfd, "p", 2, 0);
    if(n < 0) syserr("can't send to server");
    //printf("send...\n");
  
    n = recv(sockfd, buffer, 255, 0);
    if(n < 0) syserr("can't receive from server");
    //else buffer[n] = '\0';

    usleep(sleeptime);
  }
  
  send(sockfd, "quit", 5, 0);
  recv(sockfd, buffer, 255, 0);
  close(sockfd);
  printf("done...\n");

  return 0;
}
