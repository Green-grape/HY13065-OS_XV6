#include "types.h" //없으면 defs.h에서 필요한 types가 존재하지 않아 실행되지 않음
#include "mmu.h" //defs를 위한 parameter1
#include "param.h" //defs를 위한 parameter2
#include "defs.h"
#include "proc.h" //proc.h에서 myproc()를 사용하기 때문에 include

int getgpid(void){
    struct proc *p = myproc();
    struct proc *pp = p->parent;
    struct proc *gpp= pp->parent;
    return gpp->pid;
}

int sys_getgpid(void){
    return getgpid();
}  