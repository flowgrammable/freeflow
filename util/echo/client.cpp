#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_PACKET_SIZE 1500
 
int main(int argc, char* argv[])
{
  if (argc != 3) {
    printf("Invalid arguments. Usage: %s <address> <port>\n", argv[0]);
    return 0;
  }
  
  char sendbuf[MAX_PACKET_SIZE];
  char recvbuf[MAX_PACKET_SIZE];

  int sockfd;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof servaddr);
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(atoi(argv[2]));
  inet_pton(AF_INET, argv[1], &(servaddr.sin_addr));

  connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  while(1)
  {
    bzero(sendbuf, MAX_PACKET_SIZE);
    bzero(recvbuf, MAX_PACKET_SIZE);
    fgets(sendbuf, MAX_PACKET_SIZE,stdin);

    write(sockfd, sendbuf, strlen(sendbuf) + 1);
    read(sockfd, recvbuf, MAX_PACKET_SIZE);
    printf("%s", recvbuf);
  }
 
}