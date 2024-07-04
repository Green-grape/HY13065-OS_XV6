#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

static struct proc *initproc;
struct ptable_t ptable;
struct mlfq_t mlfq;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct proc_queue moq;//Monopoly Queue

void 
proc_queue_init(void){
  //mlfq init
  int len=sizeof(mlfq.queues)/sizeof(mlfq.queues[0]);
  for(int i = 0; i < len; i++){
    memset(mlfq.queues[i].elements, 0, sizeof(struct proc*)*(NPROC+1));
    mlfq.queues[i].front=0;
    mlfq.queues[i].end=0;
    mlfq.queues[i].size=0;
    mlfq.time_quantum[i] = 2*i+2;
  }
  mlfq.ticks=0;
  mlfq.is_blocked=0;

  //moq init
  memset(moq.elements, 0, sizeof(struct proc*)*(NPROC+1));
  moq.front=0;
  moq.end=0;
  moq.size=0;
}

//---- MLFQ ----

void mlfq_block(){
  mlfq.is_blocked=1;
}

void mlfq_unblock(){
  mlfq.is_blocked=0;
  mlfq.ticks=0;
}

int 
mlfq_is_full(int level){
  return mlfq.queues[level].size==NPROC;
}

int 
mlfq_is_empty(int level){
  return mlfq.queues[level].size==0;
}

//해당 레벨의 큐에 프로세스를 삽입
//성공시 0, 실패시 -1 반환
int 
mlfq_enqueue(struct proc* p, int priority, int level){
  if(mlfq_is_full(level)){
    return -1;
  }
  if(level<3){
    mlfq.queues[level].end = (mlfq.queues[level].end + 1) % (NPROC+1);
    mlfq.queues[level].elements[mlfq.queues[level].end]=p;
  }else{
    //우선순위큐
    int idx=mlfq.queues[level].size;
    while((idx!=0) && (mlfq.queues[level].elements[(idx-1)/2]->priority<priority)){
      mlfq.queues[level].elements[idx]=mlfq.queues[level].elements[(idx-1)/2];
      idx=(idx-1)/2;
    }
    mlfq.queues[level].elements[idx]=p;
  }
  mlfq.queues[level].size++;
  return 0;
}

//해당 레벨의 큐에서 프로세스의 주소를 반환
//실패시 0 반환
struct proc*
mlfq_dequeue(int level){
  struct proc* ret=0;
  if(mlfq_is_empty(level)){
    return ret;
  }
  if(level<3){
    mlfq.queues[level].front = (mlfq.queues[level].front + 1) % (NPROC+1);
    ret=mlfq.queues[level].elements[mlfq.queues[level].front];
  }else{
    //우선순위 큐
    int parent=0, child=1;
    ret=mlfq.queues[level].elements[0];
    struct proc* last=mlfq.queues[level].elements[mlfq.queues[level].size-1];
    while(child<=mlfq.queues[level].size-1){
      if(child<mlfq.queues[level].size-1 && mlfq.queues[level].elements[child]->priority<mlfq.queues[level].elements[child+1]->priority){
        child++;
      }
      if(last->priority>=mlfq.queues[level].elements[child]->priority){
        break;
      }
      mlfq.queues[level].elements[parent]=mlfq.queues[level].elements[child];
      parent=child;
      child=child*2+1;
    }
    mlfq.queues[level].elements[parent]=last;
  }
  mlfq.queues[level].size--;
  return ret;
}

//pid에 해당하는 프로세스를 queue에서 제거하고 해당 주소를 반환한다.
//존재하지 않는 경우 0 반환
struct proc*
mlfq_delete(int pid){
  int lvl=0;
  //MLFQ에서 해당 프로세스가 어느 큐에 있는지 찾기
  int flag=0;
  struct proc* target=0;
  for(lvl=0;lvl<4;lvl++){
    if(lvl<3){
      int i=(mlfq.queues[lvl].front+1)%(NPROC+1);
      while(1){
        if(mlfq.queues[lvl].elements[i]->pid==pid){
          target=mlfq.queues[lvl].elements[i];
          flag=1;
          break;
        }
        if(i==mlfq.queues[lvl].end) break;
        i=(i+1)%(NPROC+1);
      }
    }else{
      for(int i=0;i<mlfq.queues[lvl].size;i++){
        if(mlfq.queues[lvl].elements[i]->pid==pid){
          target=mlfq.queues[lvl].elements[i];
          flag=1;
          break;
        }
      }
    }
    if(flag) break;
  }
  //프로세스가 존재하지 않는 경우
  if(target==0) return 0;

  //MLFQ에서 해당 프로세스 제거
  struct proc* temp[NPROC];
  int count=0;
  while(!mlfq_is_empty(lvl)){
    struct proc* cur=mlfq_dequeue(lvl);
    if(cur->pid==pid){
      continue;
    }
    temp[count++]=cur;
  }
  for(int i=0;i<count;i++){
    mlfq_enqueue(temp[i], temp[i]->priority, temp[i]->level);
  }
  return target;
}

//해당 레벨의 큐에서 가장 앞의 프로세스의 주소를 반환
struct proc* mlfq_front(int lvl){
  if(mlfq_is_empty(lvl)){
    return 0;
  }
  if(lvl<3){
    return mlfq.queues[lvl].elements[(mlfq.queues[lvl].front+1)%(NPROC+1)];
  }else{
    return mlfq.queues[lvl].elements[0];
  }
}


//우선순위 큐를 다시 만든다.
//ptable lock이 걸려있어야 함
void 
mlfq_remake_priority_queue(){
  struct proc* temp[NPROC];
  int count=0;
  while(!mlfq_is_empty(3)){
    temp[count++]=mlfq_dequeue(3);
  }
  for(int i=0;i<count;i++){
    mlfq_enqueue(temp[i], temp[i]->priority, 3);
  }
}

// ----MOQ----
int 
moq_is_full(){
  return moq.size==NPROC;
}

int 
moq_is_empty(){
  return moq.size==0;
}

int moq_is_have_proc(int pid){
  if(moq_is_empty()) return 0;
  int i=(moq.front+1)%(NPROC+1);
  while(1){
    if(moq.elements[i]->pid==pid){
      return 1;
    }
    if(i==moq.end) break;
    i=(i+1)%(NPROC+1);
  }
  return 0;
}

struct proc*
moq_find_active_proc(int* active_cnt){
  if(moq_is_empty()) return 0;
  int i=(moq.front+1)%(NPROC+1);
  while(1){
    if(moq.elements[i]->state!=ZOMBIE){
      (*active_cnt)++;
    }
    if(moq.elements[i]->state==RUNNABLE || moq.elements[i]->state==RUNNING){
      return moq.elements[i];
    }
    if(i==moq.end) break;
    i=(i+1)%(NPROC+1);
  }
  return 0;
}

int moq_enqueue(struct proc* p){
  if(moq_is_full()){
    return -1;
  }
  moq.end=(moq.end+1)%(NPROC+1);
  moq.elements[moq.end]=p;
  moq.size++;
  return 0;
}

struct proc* 
moq_dequeue(){
  if(moq_is_empty()){
    return 0;
  }
  moq.front=(moq.front+1)%(NPROC+1);
  struct proc* ret=moq.elements[moq.front];
  moq.size--;
  return ret;
}

void
moq_delete(int pid){
  struct proc* temp[NPROC];
  int count=0;
  while(!moq_is_empty()){
    struct proc* cur=moq_dequeue();
    if(cur->pid==pid) continue;
    temp[count++]=cur;
  }
  for(int i=0;i<count;i++){
    moq_enqueue(temp[i]);
  }
}

//MOQ에 있던 프로세스를 MLFQ로 이동한다.
//이때 L0 queue로 이동한다. 
int move_moq_to_mlfq(){
  if(mlfq_is_full(0)) return -1;
  while(!moq_is_empty()){
    struct proc* target=moq_dequeue();
    target->level=0;
    mlfq_enqueue(target, target->priority, 0);
  }
  return 0;
}


//MLFQ에 있는 pid를 가진 프로세스를 MOQ로 옮김
//실패시 -1 반환
int move_mlfq_to_moq(int pid){
  if(moq_is_full() || moq_is_have_proc(pid)) return -1;
  struct proc* target=mlfq_delete(pid);
  if(target==0){
    return -1;
  }

  //MOQ에 삽입
  target->level=99;
  moq_enqueue(target);
  int i=(moq.front+1)%(NPROC+1);
  int active_cnt=0;
  while(1){
    if(moq.elements[i]->state!=ZOMBIE){
      active_cnt++;
    }
    if(i==moq.end) break;
    i=(i+1)%(NPROC+1);
  }
  return active_cnt;
}

void print_current_state_p(struct proc* pcur){
  if(pcur!=0){
      cprintf("Current Process: (pid:%d, time_quantum:%d)\n", pcur->pid, pcur->time_quantum);
  }
  if(!mlfq.is_blocked){
    cprintf("RR Queue: \n");
    for(int lvl=0;lvl<3;lvl++){
      cprintf("Level %d: ", lvl);
      if(!mlfq_is_empty(lvl)){
        int cur=(mlfq.queues[lvl].front+1)%(NPROC+1);
        while(1){
          cprintf("(pid:%d, time_quantum:%d, state:%d) ", mlfq.queues[lvl].elements[cur]->pid, mlfq.queues[lvl].elements[cur]->time_quantum, mlfq.queues[lvl].elements[cur]->state);
          if(cur==mlfq.queues[lvl].end){
            break;
          }
          cur=(cur+1)%(NPROC+1);
        }
      }
      cprintf("(size:%d)\n", mlfq.queues[lvl].size);
    }
    cprintf("Priority Queue: ");
    for(int i=0;i<mlfq.queues[3].size;i++){
      cprintf("(pid:%d, time_quantum:%d, state:%d, priority:%d) ", mlfq.queues[3].elements[i]->pid, mlfq.queues[3].elements[i]->time_quantum, mlfq.queues[3].elements[i]->state, mlfq.queues[3].elements[i]->priority);
    }
    cprintf("(size:%d)\n", mlfq.queues[3].size);  
  }else{
    cprintf("Monopoly Queue: ");
    if(!moq_is_empty()){
        int i=(moq.front+1)%(NPROC+1);
        while(1){
          cprintf("(pid:%d, time_quantum:%d, state:%d) ", moq.elements[i]->pid, moq.elements[i]->time_quantum, moq.elements[i]->state);
          if(i==moq.end) break;
          i=(i+1)%(NPROC+1);
        }
    }
    cprintf("(size:%d)\n", moq.size);
  }
  
}

void print_process(struct proc* p){
  if(p==0){  
    cprintf("0 Process!\n");
  }else{
      cprintf("pid: %d, name: %s, state: %d, level: %d, time_quantum: %d, priority: %d, pgdir:%d\n", p->pid, p->name, p->state, p->level, p->time_quantum, p->priority, p->pgdir);
  }
}


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  proc_queue_init();
}


// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->level = 0;
  p->time_quantum=0;
  p->priority=0;

  if(mlfq_enqueue(p, 0, 0)==-1){ //L0 queue에 삽입이 되지 않는 경우 프로세스를 생성하지 않음
    release(&ptable.lock); //To Do: 순서 이슈 작성
    return 0;
  }

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc
      ->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched(); //exit이후에 스케쥴러로 이동
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        //MLFQ에서 삭제하거나 MOQ에서 삭제
        if(mlfq_delete(p->pid)!=0){ 
          moq_delete(p->pid);
        }
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}


//RUNNABLE 프로세스를 queue에서 찾아 반환
//실패시 0 반환
struct proc* find_runnable_proc(){
  struct proc* ret=0;
  int flag=0;

  for(int lvl=0; lvl<4; lvl++){
    if(mlfq_is_empty(lvl)){
      continue;
    }
      //큐를 순회하면서 첫번째로 발견되는 RUNNABLE 프로세스를 찾음
      int size=mlfq.queues[lvl].size;
      struct proc* temp[size];
      int count=0;
      //수행할 프로세스를 찾고 큐의 가장 앞에 있도록 처리
      //이외의 프로세스는 순서를 그대로 유지
      while(!mlfq_is_empty(lvl)){
        struct proc* cur=mlfq_dequeue(lvl);
        if(cur->state==RUNNABLE && !flag){
          ret=cur;
          flag=1;
        }else{
          temp[count++]=cur;
        }
      }
      if(ret) mlfq_enqueue(ret, ret->priority, ret->level);
      for(int i=0; i<count; i++){
        mlfq_enqueue(temp[i], temp[i]->priority, temp[i]->level);
      }
      if(flag) break;
  }
  if(flag==0) ret=0; //없으면 더미 포인터 반환
  return ret;
}

// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();
//   struct proc* curproc=c->proc;
//   c->proc = 0;
//   for(;;){
//     // Enable interrupts on this processor.
//     sti();

//     // Loop over process table looking for process to run.
//     acquire(&ptable.lock);
//     p=0;
//     if(!mlfq.is_blocked){
//       p=find_runnable_proc();
//     }else{
//       p=moq_find_active_proc();
//       if(p==0){ //독점적으로 수행할 프로세스가 없는 경우
//         //unmonopolize
//         mlfq_unblock();
//       }
//     }

//     if(p!=0 && !(mlfq.is_blocked && (curproc!=0 && curproc->pid==p->pid))){
//       // Switch to chosen process.  It is the process's job
//       // to release ptable.lock and then reacquire it
//       // before jumping back to us.
//       c->proc = p;
//       switchuvm(p);
//       p->state = RUNNING;

//       swtch(&(c->scheduler), p->context);
//       switchkvm();
//       //cprintf("ticks:%d\n", mlfq.ticks);


//       if(!mlfq.is_blocked){
//         if(p->state==RUNNABLE){ //올바르게 process가 종료됨
//           p->time_quantum++;
//         }
//           //priority boosting
//         if(++mlfq.ticks==100){
//           mlfq.ticks=0;
//           struct proc* temp[NPROC];
//           int count=0;
//           for(int lvl=1; lvl<4;lvl++){
//             while(!mlfq_is_empty(lvl)){
//               temp[count++]=mlfq_dequeue(lvl);
//             }
//           }
//           for(int i=0;i<count;i++){
//             temp[i]->level=0;
//             temp[i]->time_quantum=0;
//             mlfq_enqueue(temp[i], temp[i]->priority, 0);
//           }
//         }
//         //time_quantum이 time_quantum을 넘으면 우선순위를 낮추거나 레벨을 낮추어 큐에 삽입하고 새로운 프로세스를 선택
//         else if((p->time_quantum)>=mlfq.time_quantum[p->level]){
//           int lvl=p->level;
//           if(p->level==0){
//             if(p->pid%2!=0){
//               p->level=1;
//             }else{
//               p->level=2;
//             }
//           }else{
//             if(p->level==3){
//               p->priority=p->priority>0 ? p->priority-1 : 0;
//             }
//             p->level=3;
//           }
//           p->time_quantum=0;

//           //프로세스가 수행을 완료한 경우 다시 큐에 삽입
//           //RR인 경우 1번만 수행하면 되지만
//           //RQ인 경우 큐를 순회하면서 찾아야함
//           struct proc* temp[NPROC];
//           int count=0;
//           do{
//             temp[count++]=mlfq_dequeue(lvl);
//             if(temp[count-1]->pid==p->pid){
//               break;
//             }
//           }while(1);
//           for(int i=0;i<count-1;i++){
//             mlfq_enqueue(temp[i], temp[i]->priority, lvl);
//           }
//           mlfq_enqueue(temp[count-1], temp[count-1]->priority, temp[count-1]->level);
//         }
//       }

//       // Process is done running for now.
//       // It should have changed its p->state before coming back.
//       c->proc = 0;
//     }
//     release(&ptable.lock);
//   }
// }

void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  struct proc* curproc=c->proc;
  c->proc = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    p=0;
    if(!mlfq.is_blocked){
      p=find_runnable_proc();
    }else{
      int active_cnt=0;
      p=moq_find_active_proc(&active_cnt);
      if(active_cnt==0){ //독점적으로 수행할 프로세스가 없는 경우
        //unmonopolize
        mlfq_unblock();
      }
    }

    if(p!=0 && !(mlfq.is_blocked && (curproc!=0 && curproc->pid==p->pid))){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();
      //cprintf("ticks:%d\n", mlfq.ticks);


      if(!mlfq.is_blocked){
        //MLFQ 스케쥴링
        if(p->state==RUNNABLE){ //올바르게 process가 종료됨
          p->time_quantum++;
        }
          //priority boosting
        if(++mlfq.ticks==100){
          mlfq.ticks=0;
          struct proc* temp[NPROC];
          int count=0;
          for(int lvl=1; lvl<4;lvl++){
            while(!mlfq_is_empty(lvl)){
              temp[count++]=mlfq_dequeue(lvl);
            }
          }
          for(int i=0;i<count;i++){
            temp[i]->level=0;
            temp[i]->time_quantum=0;
            mlfq_enqueue(temp[i], temp[i]->priority, 0);
          }
        }
        //time_quantum이 time_quantum을 넘으면 우선순위를 낮추거나 레벨을 낮추어 큐에 삽입하고 새로운 프로세스를 선택
        else if((p->time_quantum)>=mlfq.time_quantum[p->level]){
          int lvl=p->level;
          if(p->level==0){
            if(p->pid%2!=0){
              p->level=1;
            }else{
              p->level=2;
            }
          }else{
            if(p->level==3){
              p->priority=p->priority>0 ? p->priority-1 : 0;
            }
            p->level=3;
          }
          p->time_quantum=0;

          //프로세스가 수행을 완료한 경우 다시 큐에 삽입
          //RR인 경우 1번만 수행하면 되지만
          //RQ인 경우 큐를 순회하면서 찾아야함
          struct proc* temp[NPROC];
          int count=0;
          do{
            temp[count++]=mlfq_dequeue(lvl);
            if(temp[count-1]->pid==p->pid){
              break;
            }
          }while(1);
          for(int i=0;i<count-1;i++){
            mlfq_enqueue(temp[i], temp[i]->priority, lvl);
          }
          mlfq_enqueue(temp[count-1], temp[count-1]->priority, temp[count-1]->level);
        }
      }

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  //print_current_state_p(p);
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
