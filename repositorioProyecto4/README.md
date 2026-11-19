# Proyecto de Sistemas Operativos
## Proyecto 4: Copy-on-Write y MiniDB (xv6)

### Integrantes

- Thomas Bedoya Rendon
- David Guerra Morales
- Eder Ceballos Quiroz

### Curso

Sistemas Operativos

### Semestre

2026-2

---

# Descripción

La solución implementa tres partes sobre la base xv6-riscv (commit 06aad25 con `base_proyecto3.patch`).

**Parte A: Copy-on-Write en `fork()`.** `uvmcopy()` ya no copia páginas de datos: el hijo mapea las mismas páginas físicas del padre. Las páginas escribibles quedan con `PTE_COW` y sin `PTE_W` en ambas tablas. Cada página física tiene un conteo de referencias protegido por un spinlock propio (`refs.lock`). Cuando un proceso escribe en una página CoW, el fallo de escritura (`scause` 15) llega a `usertrap()`, que llama a `cowfault()`. Esa función copia la página si tiene más de una referencia, o solo le devuelve `PTE_W` si tiene una. `copyout()` usa la misma función, porque el kernel escribe en memoria del usuario sin pasar por el hardware. Si no hay memoria para copiar, el proceso se marca con `setkilled()` sin pánico.

**Parte B: `cowstat()`.** La llamada devuelve las páginas libres del sistema, las páginas con más de una referencia, y los fallos y copias CoW del proceso que la invoca. Los contadores `cowfaults` y `cowcopies` están en `struct proc`, se inicializan en `allocproc()` y se incrementan en `cowfault()`.

**Parte C: MiniDB.** Se completó `minidb snapshot M`. El padre calcula el checksum de la tabla, limpia los bits `PTE_A` con `pgaccess()` y hace `fork()`. El hijo escribe `snap.db` directamente desde la tabla. Como las páginas de la tabla son CoW, el hijo ve la tabla tal como estaba en el momento del `fork()`. Mientras tanto, el padre aplica `M` transferencias. Al final, el padre espera al hijo, relee el archivo, verifica el encabezado y el checksum de las cuentas, y comprueba que el saldo total se conservó.

---

# Archivos modificados

**kernel/riscv.h**: define `PTE_COW` como el bit 8 de la PTE (`1L << 8`), dentro de los bits RSW que RISC-V reserva para el software. Se agrupa con los demás flags de PTE.

**kernel/kalloc.c**: agrega el conteo de referencias por página física (`refs.count`) con su propio spinlock. `kalloc()` deja la página con una referencia. `kfree()` resta una referencia y solo devuelve la página a la lista libre cuando el conteo llega a 0. `freerange()` pone el conteo en 1 antes de liberar cada página, porque las páginas libres nunca pasaron por `kalloc()`. Se agregan `krefinc()` (suma una referencia), `krefcount()` (lee el conteo) y `kstats()` (cuenta páginas libres y compartidas para `cowstat()`).

**kernel/vm.c**: agrega `cowfault()`, que resuelve un fallo de escritura CoW. Modifica `uvmcopy()` para compartir páginas en lugar de copiarlas, y `copyout()` para resolver páginas CoW antes de escribir. En el camino de error de `uvmcopy()` se llama a `sfence_vma()`, porque durante el recorrido ya se había quitado `PTE_W` a páginas del padre. `cowfault()` cuenta los fallos y las copias del proceso. También contiene `vmprint()` y `vmprint_rec()` de la versión del Proyecto 3, que se usa en lugar de la de la base del docente porque devuelve `int` (el enunciado pide que devuelva 0) y su formato coincide con la validación de `ejecutar_pruebas.py`.

**kernel/trap.c**: en `usertrap()`, un fallo de escritura se atiende primero con `cowfault()`. Si devuelve -2 (sin memoria), se llama a `setkilled()`. Los demás fallos siguen por `vmfault()`.

**kernel/defs.h**: prototipos de `krefinc`, `krefcount`, `kstats` y `cowfault`.

**kernel/proc.h**: campos `cowfaults` y `cowcopies` en `struct proc`.

**kernel/proc.c**: `allocproc()` inicializa `cowfaults` y `cowcopies` en 0.

**kernel/cowstat.h**: estructura `struct cowstat` con los campos del enunciado. Archivo nuevo.

**kernel/sysproc.c**: `sys_cowstat()`, que llena la estructura con `kstats()` y con los contadores del proceso, y la copia al usuario con `copyout()`.

**kernel/syscall.h**: número `SYS_cowstat`.

**kernel/syscall.c**: declaración `extern` y entrada `[SYS_cowstat] sys_cowstat` en la tabla `syscalls[]`.

**user/minidb.c**: completa `guardar_snapshot()` y `snapshot()`, con las funciones auxiliares `saldo_total()`, `leer_cow()`, `contar_bits()` y `verificar_snapshot()`. `consulta` y `mapa` no se modificaron.

**user/user.h**: declaraciones de `struct cowstat;` y `int cowstat(struct cowstat *);`.

**user/usys.pl**: entrada `entry("cowstat");`.

**Makefile**: agrega `$U/_cowtest\` y `$U/_minidb\` a `UPROGS`.

**README.md**: este documento.

No se modificaron `user/ulib.c` ni `kernel/memlayout.h` respecto a la base del Proyecto 3.

---

# Integración con xv6

1. Clonar xv6-riscv, hacer `git switch -c proyecto4 06aad25` y aplicar `git apply base_proyecto3.patch`.
2. Copiar los archivos entregados en sus rutas.
3. Copiar los archivos del docente: user/cowtest.c, user/minidb.h, user/pgtbltest.c y user/usertests.c.
4. Compilar con `make qemu CPUS=3`.
5. Ejecutar `cowtest`, `minidb consulta 3`, `minidb snapshot 2000`, `pgtbltest` y `usertests -q`.

---

# Respuestas a las preguntas

## 1. PTE antes y después de una escritura CoW

Con la salida de `minidb mapa`, la primera página de la tabla del hijo (dirección virtual `0x5000`) tiene estas entradas:

| Momento | PTE del hijo | Bits bajos (V R W X U A D) | Bit 8 (COW) | Dirección física |
|---|---|---|---|---|
| Antes de escribir | `0x21fb39d3` | V R - - U A D | sí | `0x87ece000` |
| Después de escribir | `0x21fc18d7` | V R W - U A D | no | `0x87f06000` |

Antes de escribir, el hijo tiene la misma página física que el padre, marcada como CoW y sin `PTE_W`. El bit COW indica que la página está compartida y que no puede escribirse directamente. Cuando el hijo escribe, el procesador produce un fallo de escritura. `cowfault()` ve que la página tiene más de una referencia, reserva una página nueva, copia el contenido y apunta la PTE del hijo a la copia, con `PTE_W` y sin `PTE_COW`. Por eso cambia la dirección física: la PTE ya no apunta a la página compartida.

La PTE del padre no cambia cuando escribe el hijo. Sigue apuntando a `0x87ece000` con `PTE_COW` encendido y sin `PTE_W`. Como el conteo de esa página baja de 2 a 1, el padre no necesita copiar: si escribe después, `cowfault()` ve una sola referencia y solo le devuelve `PTE_W` y le quita `PTE_COW`.

## 2. Páginas sin PTE_W y USYSCALL en fork()

`uvmcopy()` recorre solo las direcciones de 0 a `sz`, y en cada página decide según `PTE_W`. El código del programa y la página de guarda no tienen `PTE_W`, así que se comparten sin `PTE_COW`. Si se marcaran como CoW, un proceso podría escribir en su propio código después de un `fork()`, y el bit `PTE_COW` dejaría de distinguir entre "no escribible por ahora" y "no escribible nunca".

`USYSCALL` y el trapframe están por encima de `sz` y no pasan por `uvmcopy()`. Cada proceso los crea en `proc_pagetable()`, con su propio pid en `USYSCALL`. Si `USYSCALL` se compartiera, `ugetpid()` del hijo leería la página del padre y devolvería el pid del padre. La prueba `ugetpid_hijo` de `cowtest` verifica precisamente ese caso.

## 3. Carrera en el conteo de referencias

Supongamos una página P compartida por dos procesos, A y B, con conteo 2. Si `kfree()` leyera y escribiera el conteo sin lock, podría ocurrir esto:

- CPU 1 ejecuta el `exit()` de B y llama a `kfree(P)`: lee el conteo 2.
- CPU 2 ejecuta el `exit()` de A y llama a `kfree(P)`: también lee 2, porque CPU 1 todavía no ha escrito.
- CPU 1 escribe 1. CPU 2 escribe 1.

Las dos escrituras dejan el conteo en 1, aunque ya no quede ningún dueño. La página nunca llega a 0 y nunca vuelve a la lista libre: se pierde para siempre.

El caso contrario es más grave. Supongamos que A y B comparten P con conteo 2, y que B termina mientras A ejecuta `fork()`:

- CPU 1 ejecuta `kfree(P)` de B: lee 2.
- CPU 2 ejecuta `krefinc(P)` del `fork()` de A: lee 2.
- CPU 2 escribe 3. CPU 1 escribe 1.

Ahora el conteo es 1, pero hay dos dueños, A y el hijo C. Cuando A termina, su `kfree(P)` baja el conteo a 0 y libera P, mientras C todavía la mapea. C leería o escribiría una página que ya está en la lista libre.

En el código actual, `kfree()` y `krefinc()` toman `refs.lock` antes de leer y escribir el conteo, así que cada operación es atómica respecto de las demás. La decisión de liberar se toma dentro de ese lock, y la página se pone en la lista libre fuera de él, pero solo cuando el conteo ya es 0 y no queda ningún dueño. Por eso el lock basta.

Hay un caso fuera de `kfree()`. `freerange()` escribe el conteo sin lock, pero solo se ejecuta durante el arranque, con una sola CPU activa.

## 4. Consistencia del snapshot

Con `minidb snapshot 2000`, la salida es:

```
snapshot: checksum_inicial=0x6915D295
snapshot: transferencias=2000 aplicadas=1978 rechazadas=22
snapshot: paginas_tocadas=30 fallos_cow=30 copias_cow=30
snapshot: archivo bytes=262160 checksum=0x6915D295 coincide=si
snapshot: checksum_actual=0x4AB3E209 total_conservado=si
snapshot: OK
```

`snap.db` tiene el checksum de antes del `fork()` porque el hijo escribe las cuentas en el momento en que se crea. Las transferencias del padre llegan después. Cada página de la tabla que el padre escribe se copia antes de cambiarla, y el hijo conserva la página original, así que el archivo guarda el estado congelado.

`copias_cow` es menor que 64 porque se copian solo las páginas que el padre escribe. La salida muestra `paginas_tocadas=30` y `copias_cow=30`: cada página tocada se copia exactamente una vez, en su primera escritura. De las 30 páginas tocadas, 8 corresponden a las cuentas activas (las primeras 1024 cuentas ocupan 8 páginas). Las demás provienen de las transferencias que eligen cuentas fuera de ese rango, que ocurren una de cada 256 veces. Cada página tocada se copia una vez, en su primera escritura, por eso copias_cow coincide con paginas_tocadas.

Si el padre esperara al hijo antes de aplicar las transferencias, `copias_cow` sería 0. Cuando el hijo termina, su tabla de páginas se libera y el conteo de cada página vuelve a 1. Al escribir, el padre encuentra una sola referencia y no copia: solo recupera `PTE_W`. Los fallos de escritura seguirían ocurriendo, porque la PTE del padre conserva `PTE_COW` hasta que el padre escribe, pero no habría copias.

## 5. Aceleración y planificador

Estas son las salidas de `minidb consulta K` (campo `ticks`):

| K | CPUS=1 | CPUS=3 | Aceleración con CPUS=3 |
|---|---|---|---|
| 1 | 18 | 19 | 1,0 |
| 2 | 19 | 10 | 1,9 |
| 3 | 18 | 7 | 2,7 |
| 4 | 19 | 7 | 2,7 |
| 8 | 19 | 7 | 2,7 |

El trabajo total es casi el mismo para cualquier K: la firma de auditoría cuesta unos 19 ticks de CPU en una sola CPU. Con `CPUS=1` el tiempo es el mismo para todos los valores de K, porque las CPU no pueden hacer más de una cosa a la vez. El planificador Round Robin reparte el tiempo en intervalos de un tick entre los procesos listos, pero no crea capacidad de cómputo nueva: con una sola CPU, K trabajadores se turnan y el tiempo total no cambia.

Con `CPUS=3`, el tiempo de la consulta depende de dos límites. El primero es el trabajador más largo, que no puede dividirse. El segundo es el trabajo total repartido entre las CPUs, que es 19/3 ≈ 6,3 ticks. Con K = 1, el límite es el trabajador único (19 ticks). Con K = 2, hay solo dos trabajadores de 9,5 ticks cada uno, así que el tiempo queda limitado por el más largo: 10 ticks. Con K = 3, 4 u 8, el límite pasa a ser el reparto entre tres CPUs, unos 6,3 ticks, que la medición redondea a 7. Pasar de 3 a 8 trabajadores no mejora nada, porque las tres CPUs ya están ocupadas y el trabajo total no cambia; además, 8 trabajadores no se reparten de forma perfecta entre 3 CPUs (3, 3 y 2), lo que da una carga máxima de 3/8 del trabajo frente a 1/3 con K = 3. La diferencia es menor que la resolución de un tick.

Las páginas libres muestran el costo de cada trabajador: `consulta: paginas_libres` antes y con trabajadores da 9 páginas por trabajador, con K = 1 a 8 (9, 18, 27, 36 y 72 páginas). Son estructuras propias de cada proceso (tablas de páginas, trapframe, `USYSCALL` y el pipe). Los datos de la tabla no se copian.

---

# Resultados de las pruebas

Las salidas completas están en `pruebas/`, generadas con `ejecutar_pruebas.py` sobre la versión entregada.

| Suite | Resultado | Notas |
|---|---|---|
| `cowtest` | 11 de 11 pruebas OK (`CPUS=3`) | Las dos líneas `usertrap(): unexpected scause 0xf` durante `solo_lectura` son escrituras que la prueba hace a propósito en direcciones de solo lectura. El proceso muere y la prueba termina OK |
| `pgtbltest` | 11 de 11 pruebas OK (`CPUS=3`) | La línea `usertrap(): ... stval=0x3fffffd000` durante `usyscall_solo_lectura` es la escritura intencional en `USYSCALL` |
| `minidb consulta K` (K = 1, 2, 3, 4, 8) | `consulta: OK` en todas | Firma `0x2DD2F610`, totales por sucursal iguales al Anexo B |
| `minidb snapshot 2000` | `snapshot: OK` | Checksum inicial `0x6915D295`, 1978 aplicadas, 22 rechazadas, 30 páginas tocadas, 30 copias CoW, archivo coincide |
| `minidb snapshot 0` | `snapshot: OK` | Sin transferencias, checksum final igual al inicial |
| `minidb mapa` | Dos volcados de `vmprint()` | La PTE de `0x5000` cambia de COW de solo lectura a escribible (ver respuesta 1) |
| `usertests -q` | `ALL TESTS PASSED` | Sin pánicos ni `lost some free pages` |

Las advertencias `riscv64-elf-ld: warning: ... LOAD segment with RWX permissions` aparecen en la base del Proyecto 3 y no en el código de este proyecto. No hay advertencias del compilador.

---

# Decisiones de diseño

- **`refs` tiene su propio spinlock**, separado de `kmem`. Así cambiar el conteo y cambiar la lista libre no se bloquean entre sí. Nunca se toman los dos a la vez, lo que evita un deadlock.
- **`kfree()` pone la página en la lista libre fuera de `refs.lock`**, pero solo cuando el conteo ya llegó a 0. En ese punto no queda ningún dueño, así que nadie más puede usar la página.
- **`freerange()` pone el conteo en 1** antes de llamar a `kfree()`, porque las páginas libres nunca pasaron por `kalloc()`. Sin eso, el conteo quedaría en -1.
- **`cowfault()` reserva antes de tocar la PTE.** Si `kalloc()` falla, la tabla queda intacta y se devuelve -2.
- **`cowfault()` se llama desde `usertrap()` y desde `copyout()`.** La lógica está en un solo lugar.
- **`copyout()` lee la dirección física después de `cowfault()`.** Si la leyera antes, escribiría en la página que ya dejó de pertenecer al proceso.
- **`sfence_vma()` después de cambiar PTEs** en `cowfault()`, `uvmcopy()` (incluido su camino de error) y `pgaccess()`. Así la TLB no conserva permisos viejos.
- **`walk(..., 0)` en `pgaccess()` y en `cowfault()`**, para que una consulta no cree tablas ni páginas. Las páginas de `sbrklazy()` sin tocar no reciben memoria.
- **`uvmcopy()` solo comparte páginas con `PTE_W`.** Las demás (código, guarda) se comparten sin `PTE_COW`.
- **`vmprint()` del Proyecto 3** en lugar de la de la base del docente, por el tipo de retorno y el formato validado por el script.
- **`minidb snapshot` relee el archivo con un buffer de `sbrk()`**, porque la pila del programa es de una sola página.

---

# Limitaciones conocidas

- **`pgaccess()` limpia `PTE_A` antes de copiar el resultado con `copyout()`.** Si la dirección del resultado es inválida, la llamada devuelve -1, pero los bits ya se perdieron y la siguiente consulta no los reporta.
- **La verificación de `cowfault()` no es atómica.** `cowfault()` consulta el conteo y después actúa. Entre ambas cosas, otro proceso puede terminar y bajar el conteo. El resultado es una copia de más, nunca una pérdida de datos, Si el conteo es 1, solo el proceso que falla tiene mapeada la página, así que nadie puede subirlo. Si es mayor que 1 y baja mientras se atiende el fallo, se hace una copia de más, pero nunca se pierde un dato.
- **`kstats()` cuenta las páginas libres recorriendo la lista**, así que `cowstat()` cuesta O(n) en el número de páginas libres. Es aceptable para estas pruebas, pero no para un kernel de producción.
- **El arreglo `refs.count` ocupa unos 128 KB** dentro del kernel, lo que reduce las páginas libres en el arranque.
- **El camino de error de `uvmcopy()` no se ejerce con las pruebas del docente**, porque requiere quedarse sin memoria a mitad del recorrido. Se revisó por lectura del código.
- **`minidb snapshot` sobrescribe `snap.db`** en cada ejecución (`O_TRUNC`). No hay rotación de copias.
- **MiniDB no recupera la tabla desde `snap.db`** al iniciar, como indica el enunciado.

---

# Declaración de uso de IA

Se usó Claude (Anthropic) para revisar la implementación contra la lista de chequeo, detectar si faltaba alguno de los requerimientos, limpiar comentarios y cambios de formato ajenos al proyecto, verificar la compilación y las pruebas sobre una copia limpia del commit 06aad25, y redactar este README. Los integrantes asumen plena responsabilidad académica sobre el contenido entregado.

