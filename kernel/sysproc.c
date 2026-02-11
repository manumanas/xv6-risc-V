#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void)
{
  uint64 addr;
  int length;
  int prot;
  int flags;
  int fd;
  int offset;

  struct file *f;
  struct proc *p = myproc();

  // fetch syscall arguments (NO return value checks)
  argaddr(0, &addr);
  argint(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);

  // lab guarantees these, but we still validate
  if(addr != 0 || offset != 0)
    return -1;

  if(fd < 0 || fd >= NOFILE)
    return -1;

  f = p->ofile[fd];
  if(f == 0)
    return -1;

  // find free VMA slot
  int i;
  for(i = 0; i < NVMA; i++){
    if(p->vmas[i].used == 0)
      break;
  }

  if(i == NVMA)
    return -1;

  uint64 sz = PGROUNDUP(length);

  // choose a virtual address (top-down)
  uint64 base = MAXVA - PGSIZE;
  for(int j = 0; j < i; j++){
    if(p->vmas[j].used)
      base -= PGROUNDUP(p->vmas[j].len);
  }
  base = PGROUNDDOWN(base - sz);

  // fill VMA
  p->vmas[i].used  = 1;
  p->vmas[i].addr  = base;
  p->vmas[i].len   = sz;
  p->vmas[i].prot  = prot;
  p->vmas[i].flags = flags;
  p->vmas[i].file  = f;

  filedup(f);

  return base;
}
uint64
sys_munmap(void)
{
  return -1;
}


