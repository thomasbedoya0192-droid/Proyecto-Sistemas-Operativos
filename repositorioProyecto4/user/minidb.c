// minidb: base de datos de cuentas en memoria (Proyecto 4).
// Sistemas Operativos 2026-2, Universidad EAFIT.
//
// ESQUELETO entregado por el docente. Ya están hechos:
//   minidb consulta K     reporte por sucursal con K procesos trabajadores
//   minidb mapa           vmprint() de un hijo antes y después de escribir
// El grupo debe implementar (busquen "TODO"):
//   minidb snapshot M     guarda snap.db en un hijo mientras el padre
//                         aplica M transferencias (sección C.2 del enunciado)
//
// Estudien consulta(): es el modelo de multiprogramación del proyecto
// (varios hijos con fork(), un pipe por hijo, el padre combina).
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/fcntl.h"
#include "kernel/cowstat.h"
#include "user/user.h"
#include "user/minidb.h"

static struct cuenta *tabla;

static void
morir(char *msg)
{
  printf("minidb: error: %s\n", msg);
  exit(1);
}

static void
cargar_tabla(void)
{
  if((tabla = reservar_tabla()) == 0)
    morir("no hay memoria para la tabla");
  for(int i = 0; i < NCUENTAS; i++)
    generar_cuenta(i, &tabla[i]);
}

static uint64
libres(void)
{
  struct cowstat st;
  if(cowstat(&st) < 0)
    morir("cowstat() falló");
  return st.freepages;
}

static void
agregar(struct resultado *r, int desde, int hasta, int con_firma)
{
  memset(r, 0, sizeof(*r));
  r->desde = desde;
  r->hasta = hasta;
  for(int i = desde; i < hasta; i++){
    struct cuenta *c = &tabla[i];
    struct agregado *a = &r->suc[c->sucursal];
    a->cuentas++;
    a->saldo_total += c->saldo;
    if(c->saldo > a->saldo_max)
      a->saldo_max = c->saldo;
    if(con_firma)
      a->firmas += firma(c);
  }
}

static int
leer_todo(int fd, void *buf, int n)
{
  int total = 0;
  while(total < n){
    int r = read(fd, (char *)buf + total, n - total);
    if(r <= 0)
      return -1;
    total += r;
  }
  return total;
}

static int
escribir_todo(int fd, void *buf, int n)
{
  int total = 0;
  while(total < n){
    int r = write(fd, (char *)buf + total, n - total);
    if(r <= 0)
      return -1;
    total += r;
  }
  return total;
}

static void
consulta(int k)
{
  if(k < 1 || k > MAXWORKERS)
    morir("K debe estar entre 1 y 8");
  cargar_tabla();
  printf("consulta: trabajadores=%d\n", k);

  int fds[MAXWORKERS][2];
  uint64 libres0 = libres();
  int t0 = uptime();
  for(int w = 0; w < k; w++){
    if(pipe(fds[w]) < 0)
      morir("pipe() falló");
    int pid = fork();
    if(pid < 0)
      morir("fork() falló");
    if(pid == 0){
      close(fds[w][0]);
      struct resultado r;
      agregar(&r, w * NCUENTAS / k, (w + 1) * NCUENTAS / k, 1);
      r.pid = ugetpid();
      if(escribir_todo(fds[w][1], &r, sizeof(r)) < 0)
        exit(1);
      exit(0);
    }
    close(fds[w][1]);
  }
  uint64 libres1 = libres();

  struct agregado total[NSUCURSALES];
  memset(total, 0, sizeof(total));
  int ok = 1, cubiertas = 0;
  for(int w = 0; w < k; w++){
    struct resultado r;
    if(leer_todo(fds[w][0], &r, sizeof(r)) < 0)
      morir("un trabajador no envió su resultado");
    close(fds[w][0]);
    printf("trabajador pid=%d cuentas=[%d,%d)\n", r.pid, r.desde, r.hasta);
    if(r.desde != w * NCUENTAS / k || r.hasta != (w + 1) * NCUENTAS / k)
      ok = 0;
    cubiertas += r.hasta - r.desde;
    for(int s = 0; s < NSUCURSALES; s++){
      total[s].cuentas += r.suc[s].cuentas;
      total[s].saldo_total += r.suc[s].saldo_total;
      total[s].firmas += r.suc[s].firmas;
      if(r.suc[s].saldo_max > total[s].saldo_max)
        total[s].saldo_max = r.suc[s].saldo_max;
    }
  }
  for(int w = 0; w < k; w++){
    int estado;
    if(wait(&estado) < 0 || estado != 0)
      ok = 0;
  }
  int t1 = uptime();

  // Verificación secuencial (sin la firma, que es la parte costosa).
  struct resultado sec;
  agregar(&sec, 0, NCUENTAS, 0);
  uint firmas = 0;
  for(int s = 0; s < NSUCURSALES; s++){
    printf("sucursal %d: cuentas=%d total=%ld max=%d\n", s,
           total[s].cuentas, total[s].saldo_total, total[s].saldo_max);
    if(total[s].cuentas != sec.suc[s].cuentas ||
       total[s].saldo_total != sec.suc[s].saldo_total ||
       total[s].saldo_max != sec.suc[s].saldo_max)
      ok = 0;
    firmas += total[s].firmas;
  }
  if(cubiertas != NCUENTAS)
    ok = 0;
  printf("consulta: firma=0x%x\n", firmas);
  printf("consulta: paginas_libres antes=%ld con_trabajadores=%ld\n", libres0, libres1);
  printf("consulta: ticks=%d\n", t1 - t0);
  printf("consulta: %s\n", ok ? "OK" : "FALLO");
  exit(ok ? 0 : 1);
}


// TODO (Parte C.2): el hijo escribe en SNAP_ARCHIVO un struct
// snap_encabezado (con el checksum calculado ANTES del fork) seguido de
// las NCUENTAS cuentas, escritas directamente desde la tabla (sin
// copiarlas a otro buffer). Termina con exit(0) si todo salió bien.
// (__attribute__((unused)) evita la advertencia mientras no se llame.)
static void __attribute__((unused))
guardar_snapshot(uint checksum)
{
  (void)checksum;
  exit(1);
}

// TODO (Parte C.2): implementen el snapshot consistente.
//  1. Calcular checksum_tabla() y el saldo total; imprimir checksum_inicial.
//  2. Limpiar los bits PTE_A de la tabla con pgaccess() (64 páginas).
//  3. fork(): el hijo llama a guardar_snapshot(); el padre imprime la
//     línea "snapshot: hijo pid=..." y NO espera al hijo todavía.
//  4. El padre aplica m transferencias con generar_transferencia()
//     (semilla inicial 4), midiendo cowstat() antes y después.
//  5. pgaccess() para contar las páginas tocadas; imprimir las líneas
//     "transferencias=..." y "paginas_tocadas=...".
//  6. wait(), releer snap.db (usen sbrk() para el buffer: la pila es de
//     una sola página), verificar encabezado y checksum, verificar que el
//     saldo total se conservó e imprimir "snapshot: OK" o "snapshot: FALLO".
// El formato exacto de cada línea está en el Anexo B del enunciado.
static void
snapshot(int m)
{
  uint64 mask = 0;
  cargar_tabla();
  uint checksum = checksum_tabla(tabla, NCUENTAS);
  uint64 saldo0 = 0;
  for(int i = 0; i < NCUENTAS; i++)
    saldo0 += tabla[i].saldo;
  printf("snapshot: checksum_inicial=0x%X\n", checksum);
  printf("snapshot: saldo_inicial=%ld\n", saldo0);

  // Limpia PTE_A de las 64 páginas de la tabla: lo que pgaccess() reporte
  // después será solo lo que toquen las transferencias.
  if(pgaccess(tabla, NCUENTAS * sizeof(struct cuenta) / PGSIZE, &mask) < 0)
    morir("pgaccess() falló");

  // Pendiente: fork, guardar_snapshot(), transferencias y verificación.
  (void)m;
  printf("snapshot: FALLO (sin implementar)\n");
  exit(1);
}


static void
mapa(void)
{
  // Una tabla pequeña para que la salida de vmprint() sea corta.
  char *t = sbrk(2 * PGSIZE);
  if(t == (char *)-1)
    morir("sbrk() falló");
  t[0] = t[PGSIZE] = 1;
  printf("mapa: tabla en 0x%lx (2 páginas)\n", (uint64)t);
  int pid = fork();
  if(pid < 0)
    morir("fork() falló");
  if(pid == 0){
    printf("mapa: hijo antes de escribir\n");
    vmprint();
    t[0] = 2;
    printf("mapa: hijo después de escribir en 0x%lx\n", (uint64)t);
    vmprint();
    exit(0);
  }
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  if(argc >= 2 && strcmp(argv[1], "consulta") == 0)
    consulta(argc >= 3 ? atoi(argv[2]) : 1);
  if(argc >= 2 && strcmp(argv[1], "snapshot") == 0)
    snapshot(argc >= 3 ? atoi(argv[2]) : 2000);
  if(argc >= 2 && strcmp(argv[1], "mapa") == 0)
    mapa();
  printf("uso: minidb consulta K | minidb snapshot M | minidb mapa\n");
  exit(1);
}
