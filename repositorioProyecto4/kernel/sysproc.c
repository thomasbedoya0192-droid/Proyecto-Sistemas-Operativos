#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "cowstat.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
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

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > USYSCALL)
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
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
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
sys_vmprint(void)
{
  vmprint(myproc()->pagetable);
  return 0;
}

uint64
sys_pgaccess(void)
{
  uint64 base, umask, mask = 0;
  int npages;
  struct proc *p = myproc();

  argaddr(0, &base);
  argint(1, &npages);
  argaddr(2, &umask);
  if (base % PGSIZE != 0 || npages < 1 || npages > 64)
    return -1;
  if (base + (uint64)npages * PGSIZE > p->sz)
    return -1;
  for (int i = 0; i < npages; i++) {
    pte_t *pte = walk(p->pagetable, base + (uint64)i * PGSIZE, 0);
    // páginas sin memoria física (p. ej. perezosas) cuentan como no accedidas
    if (pte == 0 || (*pte & PTE_V) == 0)
      continue;
    if (*pte & PTE_A) {
      mask |= 1L << i;
      *pte &= ~PTE_A;
    }
  }
  if (copyout(p->pagetable, p->sz, umask, (char *)&mask, sizeof(mask)) < 0)
    return -1;
  return 0;
}

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
sys_cowstat(void)
{
  uint64 addr;
  struct cowstat st;
  struct proc *p = myproc();

  argaddr(0, &addr);
  kstats(&st.freepages, &st.sharedpages);
  st.cowfaults = p->cowfaults;
  st.cowcopies = p->cowcopies;
  if(copyout(p->pagetable, p->sz, addr, (char *)&st, sizeof(st)) < 0)
    return -1;
  return 0;
}
