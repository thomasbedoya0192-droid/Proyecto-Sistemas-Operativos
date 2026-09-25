#include "types.h"
#include "riscv.h"
#include "param.h"
#include "spinlock.h"
#include "defs.h"
#include "sysinfo.h"
#include "proc.h"

int
systeminfo(uint64 addr) {
  struct proc *p = myproc();
  struct sysinfo info;

  // Llenar datos de memoria
  kmeminfo(&info.freemem_mb, &info.used_pages, &info.avail_pages);
  
  // Llenar procesos RUNNABLE
  info.runnable_proc = count_runnable();

  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}
