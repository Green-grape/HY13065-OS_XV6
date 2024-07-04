#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>


int shared_resource = 0;

#define NUM_ITERS 10
#define NUM_THREADS 10
#define MAX_THREADS 100
int number[MAX_THREADS]={0,};
int choosing[MAX_THREADS]={0,};

typedef struct{
    int tid;
}lock_args;

void lock(lock_args*);
void unlock(lock_args*);
int num_threads=0;
int num_iters=0;

// void lock(lock_args* arg)
// {
//     int isfirst=0;
//     for(int i=0;i<num_threads;i++){
//         turn[i]=arg->tid; //i번째 큐에서 실행될 프로세스
//         flag[arg->tid]=i; //현재 프로세스의 큐에서의 위치
//         while(1){
//             int is_other_process_progress=0;
//             for(int j=0;j<num_threads;j++){
//                 if(j==arg->tid) continue;
//                 if(flag[j]>=i){
//                     is_other_process_progress=1;
//                     break;
//                 }
//             }
//             int is_yield=(turn[i]!=arg->tid);
//             if(isfirst==0){
//                 isfirst=1;
//                 //printf("is_other_process_progress:%d,is_yield:%d\n", is_other_process_progress, is_yield);
//             }
//             if((i<num_threads-1) && ((!is_other_process_progress) || is_yield)) continue;
//             else break;
//         }
//         isfirst=0;
//     }
// }

// void unlock(lock_args* arg)
// {
//     printf("unlock tid:%d\n", arg->tid);
//     flag[arg->tid]=-1;
//     num_threads--;
// }

void lock(lock_args *arg){
    choosing[arg->tid]=1;
    __sync_synchronize();
    int max=0;
    for(int i=0;i<MAX_THREADS;i++){
        if(number[i]>max){
            max=number[i];
        }
    }
    __sync_synchronize();
    number[arg->tid]=1+max;
    __sync_synchronize();
    choosing[arg->tid]=0;
    for(int i=0;i<MAX_THREADS;i++){
        while(choosing[i]);
        __sync_synchronize();
        while((number[i]!=0) && ((number[i]<number[arg->tid]) || ((number[i]==number[arg->tid]) && i<arg->tid)));
    }
}

void unlock(lock_args *arg){
    number[arg->tid]=0;
}

void* thread_func(void* arg) {
    lock_args* args=(lock_args*)arg;
    lock(args);
        for(int i = 0; i < num_iters; i++)    shared_resource++;
    
    unlock(args);
    
    pthread_exit(NULL);
}

int main() {
    int n = NUM_THREADS;
    int test_cnt=0;
    srand(time(NULL));
    while(test_cnt<10000){
        test_cnt++;
        //init shared_resource
        shared_resource = 0;
        num_iters=rand()%1000;
        n=rand()%MAX_THREADS;
        pthread_t threads[n];
        int tids[n];

        printf("num_iters:%d, num_threads:%d\n", num_iters, n);
        lock_args* args=(lock_args*)malloc(sizeof(lock_args)*n);
        for (int i = 0; i < n; i++) {
            tids[i] = i;
            args[i].tid=i;
            pthread_create(&threads[i], NULL, thread_func, (void*)&args[i]);
        }
        
        for (int i = 0; i < n; i++) {
            pthread_join(threads[i], NULL);
        }
        for(int i=0;i<MAX_THREADS;i++){
            if(number[i]!=0 || choosing[i]){
                printf("(number[%d]:%d, choosing[%d]:%d) ", i, number[i], i, (int)choosing[i]);
            }
        }
        free(args);
        if(shared_resource!=n*num_iters){
            printf("shared: %d\n", shared_resource);
            break;
        }
    }
    return 0;
}