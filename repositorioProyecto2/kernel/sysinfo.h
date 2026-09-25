#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#include "types.h"

struct sysinfo {
  uint64 freemem_mb;     // Memoria libre disponible
  uint64 used_pages;     // Número de páginas utilizadas
  uint64 avail_pages;    // Número de páginas disponibles
  uint64 runnable_proc;  // Número de procesos en estado RUNNABLE
};

#endif
