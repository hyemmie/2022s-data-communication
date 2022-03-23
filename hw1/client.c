#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MAXBUF 1024
#include <stdlib.h>

void error_handling(char *msg) {
  fputs(msg, stderr);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv){

  struct sockaddr_in server_addr;
  int sockfd;
  socklen_t server_addr_len;
  char buf[MAXBUF];
  int str_len;
  int send_flag = 0;

  if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    error_handling("socket error\n");
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(60000);
  server_addr_len = sizeof(server_addr);

  if (connect(sockfd, (struct sockaddr *) &server_addr, server_addr_len) < 0) {
    close(sockfd);
    error_handling("connect error\n");
  }

  memset(buf, 0x00, MAXBUF);

  while(1) {
    fgets(buf, MAXBUF, stdin);

    if (strcmp(buf, "\n") == 0) {
      continue;
    }

    if (strcmp(buf, "Q\n") == 0 && send_flag == 1) {
      send_flag = 0;
    }

    if (strcmp(buf, "RECV\n") == 0 && send_flag == 1) {
      write(sockfd, buf, strlen(buf));
      break;
    }

    if (send_flag == 1) {
      write(sockfd, buf, strlen(buf));
    }

    if (strcmp(buf, "SEND\n") == 0 && send_flag == 0) {
      send_flag = 1;
      write(sockfd, buf, strlen(buf));
    }
  }

  while ((str_len = read(sockfd, buf, MAXBUF - 1)) != 0) {
    buf[str_len] = 0;
    printf("%s", buf);
    if (strcmp(buf, "LAST_MSG\n")) {
      break;
    }
  }

  close(sockfd);

  return 0;
}
