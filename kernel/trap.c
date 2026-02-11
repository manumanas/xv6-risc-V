#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"

#include "spinlock.h"
#include "sleeplock.h"

#include "fs.h"        // <-- NDIRECT is defined HERE
#include "file.h"      // <-- uses NDIRECT, inode, sleeplock

#include "fcntl.h"
#include "proc.h"
#include "defs.h"


struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from, and returns to, trampoline.S
// return value is user satp for trampoline.S to switch to.
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send traps to kerneltrap() while in kernel
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();

  // save user program counter
  p->trapframe->epc = r_sepc();

  if(r_scause() == 8){
    // system call
    if(killed(p))
      kexit(-1);

    p->trapframe->epc += 4;
    intr_on();
    syscall();

  } else if((which_dev = devintr()) != 0){
    // device interrupt

  } else if(r_scause() == 13 || r_scause() == 15){
    // page fault (mmap lazy allocation)

    uint64 va = PGROUNDDOWN(r_stval());
    int handled = 0;

    for(int i = 0; i < NVMA; i++){
      struct vma *v = &p->vmas[i];
      if(v->used &&
         va >= v->addr &&
         va < v->addr + v->len){

        char *mem = kalloc();
        if(mem == 0){
          setkilled(p);
          break;
        }

        memset(mem, 0, PGSIZE);

        ilock(v->file->ip);
        readi(v->file->ip, 0, (uint64)mem,
              va - v->addr, PGSIZE);
        iunlock(v->file->ip);

        int perm = PTE_U | PTE_V;
        if(v->prot & PROT_READ)  perm |= PTE_R;
        if(v->prot & PROT_WRITE) perm |= PTE_W;

        if(mappages(p->pagetable, va, PGSIZE,
                    (uint64)mem, perm) < 0){
          kfree(mem);
          setkilled(p);
        }

        handled = 1;
        break;
      }
    }

    if(!handled)
      setkilled(p);

  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n",
           r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n",
           r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    kexit(-1);

  if(which_dev == 2)
    yield();

  intr_off();
  prepare_return();
}


//
// set up trapframe and control registers for a return to user space
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 1000000 is about a tenth
  // of a second.
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

