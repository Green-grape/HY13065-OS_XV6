#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000
#define SMALL_NUM_LOOP 100
#define MED_NUM_LOOP 500

#define MAX_LEVEL 3
#define MOQ_LEVEL 99
#define PASSWORD 2019089270
int parent;

//thread만큼의 children을 생성한다. 
int
fork_children(int count){
  for (int i = 0; i < count; i++) {
    int p = fork();
    if (p == 0)
    {
      sleep(10);
      return getpid();
    }
    sleep(50);
  }
  return parent;
}

//thread만큼의 chidren을 생성하고 priority를 설정한다.
int fork_children_p(int count, int priority[]){
  for (int i = 0; i < count; i++) {
    int p = fork();
    if (p == 0)
    {
      int pid=getpid();
      sleep(300);
      printf(1, "setpriority:%d pid:%d priority:%d\n", setpriority(pid, priority[i]), pid, priority[i]);
      return getpid();
    }
    sleep(50);
  }
  return parent;
}

//parent가 아닌 모든 프로세스를 종료한다.
void exit_children()
{
  if (getpid() != parent) {
    exit();
  }
  while (wait() != -1);
}

int main(int argc, char* argv[])
{
  parent = getpid();

  printf(1, "MLFQ test start\n");
  int i = 0, pid = 0;
  int count[MAX_LEVEL+1] = { 0, 0, 0, 0 };
  char cmd = argv[1][0];
  switch (cmd) {
    case '1':
      printf(1, "[Test 1] default\n");
      //MLQF가 정상적으로 작동하는지 확인
      pid = fork_children(4);

      if (pid != parent)
      {
        for (i = 0; i < NUM_LOOP; i++)
        {
          int x = getlev();
          if (x < 0 || x > MAX_LEVEL)
          {
            printf(1, "Wrong level: %d\n", x);
            exit();
          }
          count[x]++;
        }
        printf(1, "Process %d\n", pid);
        for (i = 0; i <= MAX_LEVEL; i++)
          printf(1, "Process %d , L%d: %d\n", pid, i, count[i]);
      }
      exit_children();
      printf(1, "[Test 1] finished\n");
      break;
    case '2':
      printf(1, "[Test 2] setpriority\n");
      //priority boosting과 setpriority가 잘 작동하는지 확인
      pid=fork_children_p(4, (int[]){1,2,3,4});
      if (pid != parent)
      {
        for (i = 0; i < SMALL_NUM_LOOP; i++)
        {
          int x = getlev();
          if (x < 0 || x > MAX_LEVEL)
          {
            printf(1, "Wrong level: %d\n", x);
            exit();
          }
          count[x]++;
          printcurrentstate(PASSWORD);
        }
        printf(1, "Process %d\n", pid);
        for (i = 0; i <= MAX_LEVEL; i++)
          printf(1, "Process %d , L%d: %d\n", pid, i, count[i]);
      }
      exit_children();
      printf(1, "[Test 2] finished\n");
      break;
    case '3':
      printf(1, "[Test 3] monopolize Default\n");
      //독점모드가 잘 작동하는지 확인
      if(pid==parent){
        printf(1, "parent:%d\n", pid);
        int child=1;
        int ret=setmonopoly(child, PASSWORD);
        if(ret!=0){
          printf(1, "Monopoly Failed\n");
        }
        monopolize();
        printf(1,"monopolize\n");
        //child가 종료되고 실행되는 부분
        for(int i=0;i<SMALL_NUM_LOOP;i++){
          int x=getlev();
          printf(1, "level: %d\n", x);  
          printcurrentstate(PASSWORD);
        }
      }else{
        printf(1, "child:%d\n", pid);
        for(i=0;i<MED_NUM_LOOP;i++){
          int x=getlev();
          printf(1, "level: %d\n", x);  
          printcurrentstate(PASSWORD);
        }
        printf(1,"unmonopolize\n");
        unmonopolize();
      }
      exit_children();
      printf(1, "[Test 3] finished\n");
      break;
    case '4':
      printf(1, "[Test 4] monopolize & unmonopolize auto\n");
      //독점모드가 잘 작동하는지 확인 & 자동으로 unmonopolize가 되는지 확인
      int childcnt=2;
      pid=fork_children(childcnt);
      if(pid==parent){
        printf(1, "parent:%d\n", pid);
        int childs[childcnt];
        for(int i=0;i<childcnt;i++){
          childs[i]=pid+i+1;
        }
        for(int i=0;i<childcnt;i++){
          int ret=setmonopoly(childs[i], PASSWORD);
          if(ret!=0){
            printf(1, "Monopoly Failed\n");
          }
        }
        monopolize();
        printf(1,"monopolize\n");
        //child가 종료되고 자동으로 unmonopolize가 호출되어 여기가 실행됨
        for(int i=0;i<SMALL_NUM_LOOP;i++){
          int x=getlev();
          printf(1, "level: %d\n", x);  
          printcurrentstate(PASSWORD);
        }
      }else{
        printf(1, "child:%d\n", pid);
        for(i=0;i<MED_NUM_LOOP;i++){
          int x=getlev();
          printf(1, "level: %d\n", x);  
          printcurrentstate(PASSWORD);
        }
        exit(); 
      }
      exit_children();
      printf(1, "[Test 4] finished\n");
      break;
    case '5':
      printf(1, "[Test 5] kill test\n");
      //child를 강제로 kill했을때 스케쥴러가 정상적으로 작동하는지 체크한다. 
      pid=fork_children(1);
      if(pid==parent){
        sleep(10);
        int child=pid+1;
        kill(child);
        printf(1,"kill pid:%d\n", child);
        for(int i=0;i<SMALL_NUM_LOOP;i++){
          int x=getlev();
          printf(1, "pid:%d level: %d\n", pid, x);  
        }
      }else{
        for(int i=0;i<MED_NUM_LOOP;i++){
          int x=getlev();
          printf(1, "pid:%d level: %d\n", pid, x);  
          printcurrentstate(PASSWORD);
        }
        exit();
      }
      exit_children();
      printf(1, "[Test 5] finished\n");
    }
  exit();
}