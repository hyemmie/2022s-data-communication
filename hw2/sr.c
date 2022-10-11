#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#define _CRT_SECURE_NO_WARNINGS

struct pk_list{
    long sn;                  /* sequence number */
    double artm;              /* sender에서 packet이 발생한 시간 */
    double t_out;             /* timeout 시간 */
    struct pk_list *link;
};

typedef struct pk_list DataQue;

/* wait to be transmitted */
DataQue* WQ_front;
DataQue* WQ_rear;

/* already transmitted, waiting ACK queue */
DataQue* TransitQ_front;
DataQue* TransitQ_rear;

struct ack_list{
    long sn;                  /* sequence number */
    double rcvtm;             /* sender가 ACK를 받게 될 시간 */
    int deliver_num;
    struct ack_list *link;
};

typedef struct ack_list AckQue;
AckQue* AQ_front;
AckQue* AQ_rear;

struct buffer_list{
    long sn;                  /* sequence number */
    double artm; 
    struct buffer_list *link;
};

typedef struct buffer_list Buffer;
Buffer* BF_front;
Buffer* BF_rear;

long seq_n = 0;             /* 패킷의 순서번호 */
long transit_pknum = 0;     /* 전송했지만 ack를 받지 못한 개수 */
long next_acksn = 0;        /* receiver의 다음 ACK 넘버 */
double cur_tm;              /* current time */
double next_pk_artm;        /* 다음 패킷 도달 예상시간 */
double t_pknum = 0;         /* 지금까지 성공적으로 전송한 패킷의 수 */
double t_delay = 0;         /* 패킷이 도달하는 데에 걸린 총 딜레이 */
                            /* dealy_avg = t_delay / t_pknum */


double simul_tm;            /* 시뮬레이션 시간 */
long N = 10000;             /* 시뮬레이션 개수 */
double timeout_len;         /* timeout 시간 */
int W;                      /* sliding window size */
float a;                    /* propagate delay / transmission delay */
float t_pk;                 /* transmission delay */
float t_pro;                /* propagate delay */
float lambda;               /* 패킷 생성 poisson rate */
float p;                    /* error probability */

int user_W = 8;
float user_defined_a = 10;
float user_lambda = 10;
float user_p = 0.05;

float random_gen(void){
    float rn;
    rn = ((float) rand() / (RAND_MAX));
    return(rn);
};

void pk_gen(double tm){
    DataQue *ptr;
    
    ptr = malloc(sizeof(DataQue));
    ptr->sn = seq_n;
    ptr->artm = tm;
    ptr->link = NULL;
    seq_n++;
    
    if (WQ_front == NULL)
        WQ_front = ptr;
    else
        WQ_rear -> link = ptr;
    WQ_rear = ptr;
};

void suc_transmission(long seqn, int del_num){
    transit_pknum -= del_num;
    DataQue* temp = TransitQ_front;
    DataQue* prev;
    if (temp != NULL && temp->sn == seqn){
        TransitQ_front = temp->link;
        free(temp);
        if (TransitQ_front == NULL)
            TransitQ_rear = NULL;
    } else {
        while (temp != NULL && temp->sn != seqn) {
            prev = temp;
            temp = temp->link;
        }
        if (temp == NULL) return;
        prev->link = temp->link;
        if (prev->link == NULL) TransitQ_rear = prev;
        free(temp);
    }
};


void re_transmit(void){
    DataQue *ptr;
    
    ptr = TransitQ_front;
    TransitQ_front = ptr->link;
    if (TransitQ_front == NULL) TransitQ_rear = NULL;
    ptr->link = WQ_front;
    WQ_front = ptr;
    
    transit_pknum--;
};

void deque_Ack(void){
    AckQue *pt;
    
    pt = AQ_front;
    AQ_front = pt->link;
    free(pt);
    if (AQ_front == NULL) AQ_rear = NULL;
};

double deque_Buffer(long seqn){
    Buffer* temp = BF_front;
    Buffer* prev;
    if (temp != NULL && temp->sn == seqn){
        BF_front = temp->link;
        double result = temp->artm;
        free(temp);
        if(BF_front == NULL) BF_rear = NULL;
        return result;
    } else {
        while (temp != NULL && temp->sn != seqn) {
            prev = temp;
            temp = temp->link;
        }
        if (temp == NULL) return -100;
        prev->link = temp->link;
        double result = temp->artm;
        if (prev->link == NULL) BF_rear = prev;
        free(temp);
        return result;
    }
}

void enque_Buffer(long seqn, double tm){
    Buffer* buf_ptr;
    
    buf_ptr = malloc(sizeof(Buffer));
    buf_ptr->sn = seqn;
    buf_ptr->artm = tm;
    buf_ptr->link = NULL;
    
    if(BF_front == NULL)
        BF_front = buf_ptr;
    else
        BF_rear->link = buf_ptr;
    BF_rear = buf_ptr;
};


void enque_Ack(long seqn, int del_num){
    AckQue *ack_ptr;
    
    ack_ptr = malloc(sizeof(AckQue));
    ack_ptr->sn = seqn;
    ack_ptr->rcvtm = cur_tm + 2*t_pro;
    ack_ptr->deliver_num = del_num;
    ack_ptr->link = NULL;
    
    if (AQ_front == NULL)
        AQ_front = ack_ptr;
    else
        AQ_rear->link = ack_ptr;
    AQ_rear = ack_ptr;
};

//waitingQ에서 transitQ로 enque
void enque_Transmit(void){
    DataQue* ptr;
    ptr = WQ_front;
    transit_pknum++;
    
    WQ_front = WQ_front->link;
    if (WQ_front == NULL) WQ_rear=NULL;
    
    if (TransitQ_front == NULL)
        TransitQ_front = ptr;
    else 
        TransitQ_rear->link = ptr;
    ptr->link = NULL;
    TransitQ_rear = ptr;
};

void transmit_pk(void){
    cur_tm += t_pk;
    WQ_front->t_out = cur_tm + timeout_len;
    enque_Transmit();
};

void receive_pk(long seqn, double tm){
    if (random_gen() > p){
        if (next_acksn == seqn) {
            int deliver_num = 1;
            next_acksn++;
            t_delay += cur_tm + t_pro - tm;
            t_pknum++;
            double artm = -100;
            while ((artm = deque_Buffer(next_acksn)) > -100){
                    t_delay += cur_tm + t_pro - artm;
                    t_pknum++;
                    deliver_num++;
                    next_acksn++;
            }
            enque_Ack(seqn, deliver_num);
        } else if (next_acksn < seqn) {
            enque_Ack(seqn, 0);
            enque_Buffer(seqn, tm);
        }
    }
};

void cur_tm_update(void){
    double tm;
    if (AQ_front == NULL)
        tm = next_pk_artm;
    else if (AQ_front->rcvtm < next_pk_artm)
        tm=AQ_front->rcvtm;
    else tm = next_pk_artm;
    if (TransitQ_front != NULL)
        if (TransitQ_front->t_out < tm)
            tm = TransitQ_front->t_out;
    if ((tm > cur_tm) && ((WQ_front==NULL) || (transit_pknum >= W)))
        cur_tm = tm;
};

void print_performance_measure(void){
    double util;
    double avg_delay;
    
    avg_delay = t_delay/t_pknum;
    util = (t_pknum*t_pk)/cur_tm;

    char result[100];   
    snprintf(result, 100, "%lf, %lf\n", avg_delay, util);
    printf("%s", result);

    char filename[200]; 
    snprintf(filename, 200, "./SR-a%.0f-p%.3f-W%d-l%.3f.txt", user_defined_a,user_p,user_W,user_lambda);

   FILE *fp = fopen(filename, "a");

    fputs(result, fp);

    fclose(fp);
};

void simulate(int inp3, float inp5, float inp6, float inp7){
    N = 10000;
    timeout_len = 100;
    W = inp3;
    t_pro = 10;
    a = inp5;
    t_pk = t_pro / a;
    lambda = inp6;
    p = inp7;
    t_pknum = 0;
    
    WQ_front = NULL;
    WQ_rear = NULL;
    TransitQ_front = NULL;
    TransitQ_rear = NULL;
    AQ_front = NULL;
    AQ_rear = NULL;
    BF_front = NULL;
    BF_rear = NULL;
    
    cur_tm = -log(random_gen()) / lambda; 
    pk_gen(cur_tm); 
    next_pk_artm = cur_tm - log(random_gen()) / lambda; 
    
    while (t_pknum <= N){
        while (AQ_front != NULL){
            if (AQ_front -> rcvtm <= cur_tm){
                suc_transmission(AQ_front->sn, AQ_front->deliver_num);
                deque_Ack();
            }
            else break;
        }
        
        if (TransitQ_front != NULL){
            double timeout_time = TransitQ_front->t_out;
            if (TransitQ_front->t_out <= cur_tm){
                re_transmit();
            }
        }
        
        while (next_pk_artm <= cur_tm){
            pk_gen(next_pk_artm);
            next_pk_artm += -log(random_gen())/lambda;
        }
        
        if ((WQ_front != NULL) && (transit_pknum < W)) {
            transmit_pk();
            receive_pk(TransitQ_rear->sn, TransitQ_rear->artm);
        }
        
        cur_tm_update();
    }
    print_performance_measure();
};

int main(void){
    unsigned int seed = (unsigned int) time(NULL);
    srand(seed);
    simulate(user_W, user_defined_a, user_lambda, user_p);
    return 0;
}
