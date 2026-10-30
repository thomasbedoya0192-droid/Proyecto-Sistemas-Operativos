# Proyecto de Sistemas Operativos

## Proyecto 3: Tablas de páginas en acción (xv6)

### Integrantes

- David Guerra Morales
- Thomas Bedoya Rendon
- Eder Ceballos Quiroz

### Curso

Sistemas Operativos

### Semestre

2026-2

---

# Descripción

Se extendió el kernel de xv6-riscv (commit `06aad25`) con tres mecanismos que usan la tabla de páginas de cada proceso:

- **Parte A: página compartida y `ugetpid()`.** Al crear un proceso, `allocproc()` reserva una página física con `kalloc()` y guarda en ella el pid (`struct usyscall`). `proc_pagetable()` la mapea en `USYSCALL` (`TRAPFRAME - PGSIZE`) con `PTE_R | PTE_U`, sin `PTE_W`. `ugetpid()` (en `user/ulib.c`) lee el pid directamente de esa dirección, sin entrar al kernel. La página se desmapea en `proc_freepagetable()` y se libera en `freeproc()`. Los límites del heap en `growproc()` y `sys_sbrk()` ahora comparan contra `USYSCALL`.
- **Parte B: `vmprint()`.** Llamada al sistema que imprime la tabla de páginas Sv39 del proceso. El recorrido es recursivo (`vmprint_rec()`, siguiendo el modelo de `freewalk()`). Solo imprime PTEs con `PTE_V`, una línea por PTE con un `" .."` por nivel, la dirección virtual inicial del rango que cubre, el valor de la PTE y la dirección física.
- **Parte C: `pgaccess()`.** Llamada al sistema que, para `npages` páginas desde `base`, arma una máscara con el bit `PTE_A` (bit 6) de cada PTE y lo limpia. Usa `walk(..., 0)`, que no asigna memoria, así que las páginas perezosas no tocadas se reportan en 0. Después de limpiar los bits ejecuta `sfence_vma()` y copia la máscara al usuario con `copyout()`. Devuelve -1 si los argumentos no son válidos.

---

# Archivos modificados

| Archivo | Cambio | Motivo |
|---|---|---|
| `kernel/memlayout.h` | `#define USYSCALL (TRAPFRAME - PGSIZE)` y `struct usyscall { int pid; }` dentro de `#ifndef __ASSEMBLER__` | Dirección y contenido de la página compartida. La guarda evita errores, porque `memlayout.h` también se incluye desde ensamblador. |
| `kernel/riscv.h` | `#define PTE_A (1L << 6)` | Bit de acceso de la especificación RISC-V, usado por `pgaccess()`. |
| `kernel/proc.h` | Campo `struct usyscall *usyscall` en `struct proc` | Guarda la dirección física de la página compartida del proceso. |
| `kernel/proc.c` | `allocproc()`: `kalloc()` de la página y escritura del pid. `freeproc()`: `kfree()` de la página. `proc_pagetable()`: `mappages()` en `USYSCALL` con `PTE_R \| PTE_U` y limpieza si falla. `proc_freepagetable()`: `uvmunmap()` de `USYSCALL`. `growproc()`: límite `USYSCALL`. | Ciclo de vida completo de la página sin fugas y sin `freewalk: leaf`. El heap no puede invadir la página. |
| `kernel/vm.c` | `vmprint()` y la función recursiva auxiliar `vmprint_rec()` | Recorrido de los tres niveles de la tabla Sv39. |
| `kernel/sysproc.c` | `sys_vmprint()`, `sys_pgaccess()`. Límite de `sys_sbrk()` ahora contra `USYSCALL`. | Implementación de las llamadas nuevas. El límite cubre también `sbrklazy()`, que no pasa por `growproc()`. |
| `kernel/syscall.h` | `SYS_vmprint 23`, `SYS_pgaccess 24` | Números de las llamadas nuevas. |
| `kernel/syscall.c` | Declaraciones `extern` y entradas en la tabla `syscalls[]` | Despacho de las llamadas nuevas. |
| `kernel/defs.h` | Prototipo `int vmprint(pagetable_t)` en la sección de `vm.c` | Permite llamarla desde `sysproc.c`. |
| `user/ulib.c` | `int ugetpid(void)` | Lee el pid de `USYSCALL` sin llamada al sistema. |
| `user/user.h` | Prototipos de `vmprint()`, `pgaccess()` (llamadas al sistema) y `ugetpid()` (sección de `ulib.c`) | Firmas exactas que exige `pgtbltest`. |
| `user/usys.pl` | `entry("vmprint")`, `entry("pgaccess")` | Genera los stubs de usuario que ejecutan `ecall`. |

---

# Integración con xv6

1. Clonar xv6-riscv y hacer `git switch -c proyecto3 06aad25`.
2. Copiar los archivos entregados en sus rutas.
3. Agregar `$U/_pgtbltest` a UPROGS en el Makefile (y copiar `user/pgtbltest.c` y la versión del curso de `user/usertests.c` entregadas por el docente).
4. Compilar con `make qemu`.
5. Ejecutar `pgtbltest` y `usertests -q`.

---

# Respuestas a las preguntas

## 1. Interpretación de vmprint()

Salida de la prueba `vmprint` de `pgtbltest` (`pruebas/pgtbltest.txt`). Los permisos salen de los 10 bits bajos de cada PTE: V=0x001, R=0x002, W=0x004, X=0x008, U=0x010, A=0x040, D=0x080.

```
page table 0x0000000087f23000
 ..0x0000000000000000: pte 0x0000000021fd2001 pa 0x0000000087f48000
 .. ..0x0000000000000000: pte 0x0000000021fd0401 pa 0x0000000087f41000
 .. .. ..0x0000000000000000: pte 0x0000000021fca05b pa 0x0000000087f28000
 .. .. ..0x0000000000001000: pte 0x0000000021fc985b pa 0x0000000087f26000
 .. .. ..0x0000000000002000: pte 0x0000000021fc9c1b pa 0x0000000087f27000
 .. .. ..0x0000000000003000: pte 0x0000000021fc94d7 pa 0x0000000087f25000
 .. .. ..0x0000000000004000: pte 0x0000000021fd1407 pa 0x0000000087f45000
 .. .. ..0x0000000000005000: pte 0x0000000021fd08d7 pa 0x0000000087f42000
 .. .. ..0x0000000000009000: pte 0x00000000202b8cd7 pa 0x0000000080ae3000
 ..0x0000003fc0000000: pte 0x0000000021fca401 pa 0x0000000087f29000
 .. ..0x0000003fffe00000: pte 0x0000000021fd1c01 pa 0x0000000087f47000
 .. .. ..0x0000003fffffd000: pte 0x0000000021fc9013 pa 0x0000000087f24000
 .. .. ..0x0000003fffffe000: pte 0x0000000021fca8c7 pa 0x0000000087f2a000
 .. .. ..0x0000003ffffff000: pte 0x000000002000184b pa 0x0000000080006000
```

Las PTEs de nivel 0 y 1 tienen flags `0x001` (solo `V`). Al no tener R, W ni X, apuntan a la tabla del siguiente nivel. Las hojas (nivel 2) son:

| VA | Flags | Permisos | Contenido |
|---|---|---|---|
| `0x0000`, `0x1000`, `0x2000` | `0x05b`, `0x05b`, `0x01b` | V R X U (A en las ya ejecutadas) | **Texto** del programa: se lee y ejecuta, pero no se escribe. |
| `0x3000` | `0x0d7` | V R W U A D | **Datos** (`.data`/`.bss`): lectura y escritura, sin ejecución. |
| `0x4000` | `0x007` | V R W, sin U | **Página de guarda** debajo de la pila. Sin `PTE_U`, el usuario no puede acceder a ella. |
| `0x5000` | `0x0d7` | V R W U A D | **Pila** de usuario. |
| `0x9000` | `0x0d7` | V R W U A D | Única página tocada de la región perezosa (`sbrklazy`, de `0x6000` a `0xd000`) que crea la prueba. En un proceso recién creado sin `sbrklazy` no aparece. Las otras 7 páginas perezosas no aparecen porque aún no tienen memoria física. |
| `0x3fffffd000` | `0x013` | V R U | **USYSCALL**: el usuario la puede leer pero no escribir ni ejecutar. |
| `0x3fffffe000` | `0x0c7` | V R W A D, sin U | **Trapframe**: el trampolín guarda aquí los registros mientras todavía usa la tabla de usuario. Solo es accesible en modo supervisor. |
| `0x3ffffff000` | `0x04b` | V R X A, sin U | **Trampolín** (`uservec`/`userret`): el código que cambia de tabla de páginas. Está mapeado en la misma VA en el kernel y en todos los procesos, y su `pa 0x80006000` está dentro del texto del kernel. |

La pila y el código tienen el bit A porque el proceso ya los usó. La página `0x2000` no lo tiene porque esa parte del código todavía no se ha ejecutado.

## 2. Página de guarda

`exec()` reserva dos páginas para la pila y llama a `uvmclear()` sobre la de abajo, que le quita `PTE_U`. La página sigue mapeada (V R W), pero el hardware no permite que el modo usuario la lea ni la escriba. Así, si el programa desborda la pila (por ejemplo, con una recursión muy profunda), el primer acceso por debajo de la pila cae en la guarda y produce un fallo de página (scause 13 o 15). `vmfault()` no lo atiende porque la dirección ya está mapeada (`ismapped()`), así que `usertrap()` imprime `unexpected scause` y mata al proceso. Sin la guarda, la pila seguiría creciendo en silencio sobre los datos del programa y los corrompería. La página no se deja sin mapear porque, si quedara libre, `sbrk`/`vmfault` podría asignarle memoria y se perdería la protección.

## 3. Otras llamadas que se pueden acelerar

Se pueden acelerar las que solo **leen datos del proceso que el kernel conoce de antemano y no cambian, o que el kernel puede actualizar en la página cada vez que cambian**:

- `getppid()` (si existiera): el padre solo cambia en `reparent()`, y el kernel podría actualizar la página en ese momento.
- `uptime()`: el kernel podría escribir `ticks` en una página compartida en cada interrupción de reloj. Así funciona el vDSO de Linux para `gettimeofday()`.
- `sbrk(0)` para consultar el tamaño actual del proceso (`p->sz`), si el kernel lo actualiza en cada cambio.

**No** se pueden acelerar las que **modifican el estado del kernel o necesitan sincronización o permisos**: `fork`, `exec`, `exit`, `wait`, `kill`, `open`, `read`, `write`, `close`, `pipe`, `sbrk(n)` con `n != 0`, `pause`, etc. Estas reservan recursos, cambian tablas de páginas o de archivos, bloquean al proceso o tocan dispositivos. Todo eso exige el modo supervisor y los locks del kernel. Una página de solo lectura únicamente sirve para que el kernel *publique* información, no para que el usuario pida acciones.

## 4. Riesgo de USYSCALL con PTE_W

El proceso podría escribir cualquier valor en `struct usyscall`. `ugetpid()` dejaría de ser confiable: un proceso podría hacerse pasar por otro pid ante sus propias bibliotecas, o corromper el valor por error con un puntero mal usado. El kernel nunca confía en esa página (siempre usa `p->pid`), así que no se escalarían privilegios dentro del kernel. Aun así, se pierde la garantía de que el dato publicado es el del kernel. Si en el futuro se publicara ahí algo que el kernel también lea, sería una vía directa para que el usuario manipulara estado del kernel. Además, `copyout()` podría escribir en esa página si el usuario le pasara su dirección como destino. Por eso se mapea sin `PTE_W` y cualquier escritura mata al proceso (`usyscall_solo_lectura`).

## 5. pgaccess() y páginas perezosas

`sbrklazy()` solo aumenta `p->sz`, sin reservar memoria ni crear PTEs. La página recibe memoria física en `vmfault()` la primera vez que se toca. Mientras tanto no existe ninguna PTE válida, así que el hardware no pudo encender `PTE_A`: la página no fue accedida y lo correcto es reportar 0. `pgaccess()` usa `walk(pagetable, va, 0)`, que devuelve 0 si falta una tabla intermedia y no crea nada, y además verifica `PTE_V`. Asignarle memoria iría en contra del propósito de la asignación perezosa: consultar el bit de acceso (una operación de solo lectura que usan, por ejemplo, el reemplazo de páginas o un recolector de basura) gastaría memoria física en páginas que el programa quizás nunca use. Además, la página nueva quedaría mapeada como si hubiera sido usada, y eso falsearía la medición. La prueba `pgaccess_perezoso` verifica que la página siga apareciendo en 0 y que, al tocarla, sí se detecte.

---

# Resultados de las pruebas

Ejecutadas con `ejecutar_pruebas.py` sobre una copia limpia de xv6 en el commit `06aad25` con estos archivos copiados encima (salida completa en `pruebas/`):

- **pgtbltest**: `TODAS LAS PRUEBAS PASARON (11 de 11)`. Revisión automática del formato de `vmprint()`: OK. El mensaje `usertrap(): unexpected scause 0xf ... stval=0x3fffffd000` es el esperado en `usyscall_solo_lectura`.
- **usertests -q** (versión del curso): `ALL TESTS PASSED`, sin `lost some free pages` ni pánicos del kernel.
- El kernel compila con `-Wall -Werror` sin advertencias del compilador. El único mensaje es el aviso del enlazador `LOAD segment with RWX permissions`, que también aparece en xv6 sin modificar.

---

# Decisiones de diseño

- **Quién libera la página USYSCALL.** `proc_freepagetable()` solo la desmapea (`do_free = 0`) y `freeproc()` la libera con `kfree()`, igual que el trapframe. Así, `proc_freepagetable()` sigue sirviendo en `exec()` para liberar la tabla vieja sin liberar la página que el proceso conserva. La nueva tabla de `exec()` vuelve a mapear la misma página física, y por eso `ugetpid()` funciona después de `exec()`.
- **Orden en `allocproc()`.** La página se reserva antes de `proc_pagetable()`, que necesita su dirección física. Si `kalloc()` falla, `freeproc()` limpia lo que ya se había reservado. Si falla el `mappages()` de USYSCALL, `proc_pagetable()` deshace los mapeos anteriores.
- **Límite del heap.** Se cambió en `growproc()` (sbrk normal) y en `sys_sbrk()` (rama perezosa), porque `sbrklazy()` no pasa por `growproc()`.
- **`vmprint()` recursivo.** Igual que `freewalk()`: una PTE válida sin R/W/X apunta a otra tabla. La VA se arma con `va_base | (i << (12 + 9*(2-level)))`, que da el inicio del rango de 1 GiB, 2 MiB o 4 KiB según el nivel.
- **Validación de `pgaccess()`.** El rango se valida con `base >= p->sz || npages*PGSIZE > p->sz - base` en lugar de `base + npages*PGSIZE > p->sz`. Así, un `base` cercano a 2^64 no puede desbordar la suma, pasar la validación y hacer que `walk()` entre en pánico por `va >= MAXVA`. La máscara se escribe con `copyout()`, que rechaza destinos sin `PTE_W` o fuera del proceso.
- **`sfence_vma()` en `pgaccess()`.** Después de limpiar `PTE_A` se invalida la TLB, para que el siguiente acceso vuelva a pasar por la PTE y el hardware encienda el bit otra vez.

---

# Limitaciones conocidas

- `pgaccess()` acepta como máximo 64 páginas por llamada, porque la máscara es un `uint64`.
- Encender `PTE_A` depende del hardware (o de QEMU). En implementaciones RISC-V que generan un fallo de página en lugar de encender A/D, se necesitaría un manejador adicional.
- La página compartida solo publica el pid. Agregar más campos implicaría actualizarlos en cada cambio (por ejemplo, en `reparent()`).
- `vmprint()` imprime sin tomar locks. Es seguro porque solo recorre la tabla del proceso que hace la llamada, y esa tabla no cambia mientras el proceso está dentro de la llamada.

---

# Declaración de uso de IA

Se usó Claude (Anthropic) para revisar la implementación contra la lista de chequeo, detectar y corregir el posible desbordamiento en la validación de rango de `pgaccess()`, limpiar comentarios y cambios de formato ajenos al proyecto, verificar la compilación y las pruebas sobre una copia limpia del commit `06aad25`, y redactar este README. Los integrantes asumen plena responsabilidad académica sobre el contenido entregado.
