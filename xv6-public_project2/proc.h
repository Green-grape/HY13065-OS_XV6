#ifndef _PROC_H_
#define _PROC_H_
#include "spinlock.h"
#define PW 2019089270

// Per-CPU state

struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};


extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  int level;                   // queue level
  uint time_quantum;            // time quantum
  int priority;                // priority
  char name[16];               // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

typedef struct proc_queue{
  struct proc* elements[NPROC+1];
  int front;
  int end;
  int size;
} proc_queue;


struct ptable_t{
  struct spinlock lock;
  struct proc proc[NPROC]; 
};

struct mlfq_t{
  struct proc_queue queues[4];
  struct spinlock lock;
  int time_quantum[4];
  int ticks;
  int is_blocked;
} ;


extern struct ptable_t ptable;
extern struct mlfq_t mlfq; //Multi-Level Feedback Queue

extern void mlfq_remake_priority_queue();
extern int moq_is_have_proc(int);
extern void proc_queue_init(void);
extern int move_mlfq_to_moq(int);
extern int move_moq_to_mlfq();
extern void mlfq_block(void);
extern void mlfq_unblock(void);
extern void print_current_state_p(struct proc* p);

#endif