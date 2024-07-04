#include "types.h" //없으면 defs.h에서 필요한 types가 존재하지 않아 실행되지 않음
#include "mmu.h" //defs를 위한 parameter1
#include "param.h" //defs를 위한 parameter2
#include "defs.h"
#include "proc.h" //proc.h에서 myproc()를 사용하기 때문에 include

int 
getlev(void){
    return myproc()->level;
}

int setpriority(int pid, int priority){
  if(priority<0 || priority>10) return -2;
  struct proc* p;
  int ret=-1;
  acquire(&ptable.lock);
  //MLFQ에서 해당 프로세스의 우선순위를 변경
  for(int i = 0; i < NPROC; i++){
      p = &ptable.proc[i];
      if(p->pid == pid){
          p->priority = priority;
          if(p->level==3){
            mlfq_remake_priority_queue();
          }
          ret = 0;
      }
  }
  release(&ptable.lock);
  return ret;
}

int setmonopoly(int pid, int password){
  if(password!=PW) return -2;
  if(moq_is_have_proc(pid)) return -3;
  if(myproc()->pid==pid) return -4;
  acquire(&ptable.lock);
  int ret=move_mlfq_to_moq(pid);
  release(&ptable.lock);
  return ret;
}

//MoQ의 프로세스가 CPU를 독점하도록한다.
void monopolize(void){
  int pid=myproc()->pid;
  acquire(&ptable.lock);
  mlfq_block();
  if(moq_is_have_proc(pid)){
    release(&ptable.lock);
    return; //이미 실행되고 있는 경우 
  }
  release(&ptable.lock);
  yield();
}

//MoQ의 프로세스가 CPU 독점을 해제하고 MLFQ로 돌아간다.
void unmonopolize(void){
  acquire(&ptable.lock);
  mlfq_unblock();
  move_moq_to_mlfq();
  release(&ptable.lock);
  yield();
}

void printcurrentstate(int pw){
  if(pw!=PW) return;
  acquire(&ptable.lock);
  print_current_state_p(0);
  release(&ptable.lock);
}

//--SYS_CALL--

// 자신이 점유한 cpu를 양보합니다. 
void sys_yield(void)
{
  yield();
}

//현재 프로세스가 속한 큐의 레벨 반환
int sys_getlev(void)
{
  return getlev();
}

//프로세스의 우선순위를 설정
int sys_setpriority(void)
{
  int pid,priority;
  if(argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;
  return setpriority(pid, priority);
}

//프로세스를 MoQ로 이동
int sys_setmonopoly(void)
{
  int pid,password;
  if(argint(0, &pid) < 0 || argint(1, &password) < 0)
    return -1;
  return setmonopoly(pid, password);
}

//프로세스가 CPU를 독점하도록 설정
int sys_monopolize(void)
{
  monopolize();
  return 0;
}

//프로세스가 CPU 독점을 해제하고 MLFQ로 돌아간다.
int sys_unmonopolize(void)
{
  unmonopolize();
  return 0;
}

int sys_printcurrentstate(void){
  int password;
  if(argint(0, &password) < 0)
    return -1;
  printcurrentstate(password);
  return 0;
}