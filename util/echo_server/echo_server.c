#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#define MAX_PACKET_SIZE 1500
 
int main(int argc, char* argv[])
{
  if (argc != 2) {
    printf("Invalid arguments. Usage: %s <port>\n", argv[0]);
    return 0;
  }

  char buf[MAX_PACKET_SIZE];
  int listen_fd, comm_fd;
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htons(INADDR_ANY);
  servaddr.sin_port = htons(atoi(argv[1]));

  bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));

  listen(listen_fd, 1000);

  comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);

  while(1)
  {
    bzero(buf, MAX_PACKET_SIZE);
    read(comm_fd, buf, MAX_PACKET_SIZE);
    write(comm_fd, buf, strlen(buf) + 1);
  }
}
