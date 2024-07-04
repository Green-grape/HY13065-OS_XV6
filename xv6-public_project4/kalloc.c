// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
void incr_refc(uint);
void decr_refc(uint);
int get_refc(uint);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
  int refc[PHYSTOP/PGSIZE];
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  memset(kmem.refc, 0, sizeof(kmem.refc));
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE){
    kfree(p);
  }
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP){
    if(kmem.use_lock)
      release(&kmem.lock);
    panic("kfree");
  }
  r = (struct run*)v;
  if(kmem.refc[((uint)V2P(v))/PGSIZE]>1){
    //cprintf("freepage:%d refc:%d\n",(uint)V2P(v)/PGSIZE, kmem.refc[((uint)V2P(v))/PGSIZE]);
    kmem.refc[((uint)V2P(v))/PGSIZE]--;
    if(kmem.use_lock)
      release(&kmem.lock);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.refc[((uint)V2P(v))/PGSIZE]=0;
  //cprintf("kfree: Freed page at address %p\n", v);

  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    if(kmem.refc[(uint)V2P(r)/PGSIZE]!=0)
      panic("kalloc: page alreay allocated");
    kmem.freelist = r->next;
    kmem.refc[(uint)V2P(r)/PGSIZE]=1;
  }  
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

//pa=page의 physical address
void incr_refc(uint pa) {
  char* v=(char*)P2V(pa);
  if((uint)v%PGSIZE || v<end || V2P(v)>=PHYSTOP)
    panic("incr_refc");
  if(kmem.use_lock)
    acquire(&kmem.lock);
  //cprintf("page:%d, refc:%d incr\n",pa/PGSIZE, kmem.refc[pa/PGSIZE]);
  kmem.refc[pa/PGSIZE]++;
  if(kmem.use_lock)
    release(&kmem.lock);
}

void decr_refc(uint pa) {
  char *v=(char*)P2V(pa);
  if((uint)v%PGSIZE || v<end || V2P(v)>=PHYSTOP)
    panic("decr_refc");
  if(kmem.use_lock)
    acquire(&kmem.lock);
  kmem.refc[pa/PGSIZE]--;
  //cprintf("page:%d, refc:%d decr\n",pa/PGSIZE, kmem.refc[pa/PGSIZE]);
  if(kmem.use_lock)
    release(&kmem.lock);
}

int get_refc(uint pa) {
  char* v=(char*)P2V(pa);
  if((uint)v%PGSIZE || v<end || V2P(v)>=PHYSTOP)
    panic("get_refc");
  if(kmem.use_lock)
    acquire(&kmem.lock);
  //cprintf("page:%d, refc:%d get_refc\n",pa/PGSIZE, kmem.refc[pa/PGSIZE]);
  int ret=kmem.refc[pa/PGSIZE];
  if(kmem.use_lock)
    release(&kmem.lock);
  return ret;
}

//free page의 총 개수 반환
int countfp(void){
  //cprintf("countfp\n");
  if(kmem.use_lock)
    acquire(&kmem.lock);
  int cnt=0;
  struct run* r=kmem.freelist;
  while(r){
    cnt++;
    r=r->next;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return cnt;
}

int sys_countfp(void){
  return countfp();
}

