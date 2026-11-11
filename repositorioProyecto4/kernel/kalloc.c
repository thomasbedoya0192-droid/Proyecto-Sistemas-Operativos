// Asignador de memoria física con conteo de referencias por página.
// Una página compartida por Copy-on-Write tiene más de un dueño; solo
// vuelve a la lista libre cuando su conteo llega a cero.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[];  // primera dirección después del kernel (kernel.ld)

// Índice del conteo de una página física: cada página entre KERNBASE y PHYSTOP.
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)
#define NPAGINAS   (((uint64)PHYSTOP - KERNBASE) / PGSIZE)

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Conteo de referencias. Tiene su propio lock, separado de kmem.lock:
// nunca se toman los dos a la vez, así que no hay deadlock.
struct {
  struct spinlock lock;
  int count[NPAGINAS];
} refs;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refs.lock, "refs");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    // kfree() resta una referencia; al inicio la página debe tener 1
    // para que llegue a 0 y entre a la lista libre.
    refs.count[PA2IDX(p)] = 1;
    kfree(p);
  }
}

// Quita una referencia a la página. Solo la libera cuando el conteo llega a 0.
void
kfree(void *pa)
{
  struct run *r;
  int libre;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&refs.lock);
  if(refs.count[PA2IDX(pa)] < 1)
    panic("kfree: página sin referencias");
  refs.count[PA2IDX(pa)]--;
  libre = (refs.count[PA2IDX(pa)] == 0);
  release(&refs.lock);

  if(!libre)
    return;  // otro dueño sigue usando la página

  memset(pa, 1, PGSIZE);  // relleno para detectar referencias colgantes
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Entrega una página libre con una sola referencia.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    acquire(&refs.lock);
    refs.count[PA2IDX(r)] = 1;
    release(&refs.lock);
    memset((char*)r, 5, PGSIZE);  // relleno para detectar datos sin inicializar
  }
  return (void*)r;
}

// Suma una referencia a una página ya asignada. La usará uvmcopy().
void
krefinc(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krefinc");

  acquire(&refs.lock);
  if(refs.count[PA2IDX(pa)] < 1)
    panic("krefinc: página libre");
  refs.count[PA2IDX(pa)]++;
  release(&refs.lock);
}

//cambio 11 nov
int
krefcount(void *pa)
{
  int n;
  acquire(&refs.lock);
  n = refs.count[PA2IDX(pa)];
  release(&refs.lock);
  return n;
}
