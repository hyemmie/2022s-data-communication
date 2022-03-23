#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MAXBUF 1024
#include <pthread.h>
#include <stdlib.h>


typedef struct Node {
    char* data;
    struct Node *next;
} Node;
 
 
typedef struct Queue {
    Node *front; //맨 앞(꺼낼 위치)
    Node *rear; //맨 뒤(보관할 위치)
    int count;//보관 개수
} Queue;

void InitQueue(Queue *queue) {
    queue->front = queue->rear = NULL; //front와 rear를 NULL로 설정
    queue->count = 0;//보관 개수를 0으로 설정
}
 
int IsEmpty(Queue *queue) {
    return queue->count == 0;    //보관 개수가 0이면 빈 상태
}
 
void Enqueue(Queue *queue, char* data) {
    Node *now = (Node *)malloc(sizeof(Node)); //노드 생성
    now->data = data;//데이터 설정
    now->next = NULL;
 
    if (IsEmpty(queue))//큐가 비어있을 때
    {
        queue->front = now;//맨 앞을 now로 설정       
    }
    else//비어있지 않을 때
    {
        queue->rear->next = now;//맨 뒤의 다음을 now로 설정
    }
    queue->rear = now;//맨 뒤를 now로 설정   
    queue->count++;//보관 개수를 1 증가
}

char* Dequeue(Queue *queue) {
    char* ret = strdup("LAST_MSG\n");
    Node *now;
    if (IsEmpty(queue)) {
        return ret;
    }
    now = queue->front;
    ret = now->data;
    queue->front = now->next;
    free(now);
    queue->count--;
    return ret;
}

void error_handling(char *msg) {
  fputs(msg, stderr);
  exit(EXIT_FAILURE);
}

void* new_connection(void* sockfd) {
  int client_sock = *(int *)sockfd;
  int str_len;
  char buf[MAXBUF];
  Queue queue;
  InitQueue(&queue);
  printf("connected %d\n",client_sock);
  int save_flag = 0;

  while((str_len = read(client_sock, buf, MAXBUF)) != 0) {
    buf[str_len] = 0;

    if (strcmp(buf, "RECV\n") == 0) {
      save_flag = 0;
      break;
    }

    if (save_flag == 1 && strcmp(buf, "SEND\n") != 0) {
      char* str = strdup(buf);
      Enqueue(&queue, str);
    }

    if (strcmp(buf, "SEND\n") == 0 && save_flag == 0) {
      save_flag = 1;
    }
  }

  while(!IsEmpty(&queue)) {
    char* ret = Dequeue(&queue);
    write(client_sock, ret, strlen(ret));
  }

  write(client_sock, "LAST_MSG\n", strlen("LAST_MSG\n"));

  close(client_sock);
  printf("disconnected %d\n",client_sock);
  pthread_exit((void*)0);
}

int main(int argc, char **argv) {
  int server_sockfd, sockfd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len;
  char buf[MAXBUF];


  if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    error_handling("socket error\n");
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(60000);

  if ((bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0) {
    close(server_sockfd);
    error_handling("bind error\n");
  }

  if(listen(server_sockfd, 5) < 0){
    close(server_sockfd);
    error_handling("listen error\n");
  }

  client_addr_len = sizeof(client_addr);

  pthread_t conn_thread;

  while(1){
    if ((sockfd = accept(server_sockfd, (struct sockaddr *) &client_addr, &client_addr_len)) < 0){
      close(server_sockfd);
      error_handling("accept error\n");
    } else {
      pthread_create(&conn_thread, NULL, new_connection, (void *) &sockfd);
      pthread_detach(conn_thread);
    }
  }
}
