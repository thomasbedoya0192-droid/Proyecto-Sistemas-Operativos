#Reporte técnico: Copy-on-Write y MiniDB (xv6)

Sistemas Operativos 2026-2 · Universidad EAFIT 
Base: xv6-riscv commit 06aad25 con base_proyecto3.patch

## 1. Arquitectura de la solución

### 1.1 Ciclo de vida de una página compartida

El kernel tiene memoria física entre KERNBASE = 0x80000000 y PHYSTOP = 0x88000000. Cada página física de ese rango tiene un conteo de referencias en `refs.count`. La tabla siguiente sigue una página P, escribible, que el padre mapea en su tabla.

| Paso | Evento | PTE del padre | PTE del hijo | Conteo de P | Acción del kernel |
|---|---|---|---|---|---|
| 0 | Antes de fork | P, R W U | (no existe) | 1 | Nada |
| 1 | fork() → uvmcopy() | P, R U COW | P, R U COW | 2 | Quita W en el padre, marca COW en ambos, mapea P en el hijo y llama a krefinc() |
| 2 | El hijo escribe (scause 15) | P, R U COW | Q, R W U | 1 | cowfault(): conteo 2, así que kalloc() reserva Q, memmove() copia P en Q, la PTE del hijo apunta a Q y kfree(P) baja el conteo |
| 3 | El padre escribe | P, R W U | Q, R W U | 1 | cowfault(): conteo 1, solo devuelve PTE_W y quita PTE_COW, sin copiar |
| 4 | exit() de ambos | (liberado) | (liberado) | 0 | kfree() baja a 0 y la página vuelve a la lista libre |

En el paso 2, la PTE del padre queda con COW y sin W aunque su conteo ya sea 1. El paso 3 la restaura sin copiar.

Un caso medido en `minidb mapa`: la página 0x5000 del hijo tenía la PTE 0x21fb39d3 antes de escribir (bits V R U A D y COW = 0x100), con dirección física 0x87ece000. Después de escribir tiene la PTE 0x21fc18d7 (V R W U A D, sin COW), con dirección física 0x87f06000.

### 1.2 Protección del conteo

Hay dos locks: `refs.lock` protege `refs.count` y `kmem.lock` protege la lista libre. Ningún camino toma los dos a la vez: `kfree()` suelta `refs.lock` antes de tomar `kmem.lock`, y `kalloc()` toma `kmem.lock`, lo suelta y después toma `refs.lock`. Por eso no puede haber un deadlock entre ellos.

El lock basta porque cada decisión de liberar se toma dentro de `refs.lock`: si el conteo llega a 0 con el lock tomado, ningún mapeo cuenta ya esa página, así que nadie más puede usarla al pasarla a la lista libre. Las únicas escrituras sin lock son las de `freerange()`, que solo corre durante el arranque con una CPU activa.

## 2. Experimentos

### 2.1 Ticks de `minidb consulta K`

Un tick es una unidad del reloj del kernel (el comentario de `clockintr()` la define como aproximadamente una décima de segundo). Cada valor es de una sola ejecución. La diferencia de un tick entre K = 1 con 1 y con 3 CPUs está dentro del ruido.

| K | Ticks con CPUS=1 | Ticks con CPUS=3 | Aceleración con CPUS=3 |
|---|---|---|---|
| 1 | 18 | 19 | 1,0 |
| 2 | 19 | 10 | 1,9 |
| 3 | 18 | 7 | 2,7 |
| 4 | 19 | 7 | 2,7 |
| 8 | 19 | 7 | 2,7 |

### 2.2 Páginas libres y copias

`paginas_libres antes` fue 32 433 en todas las corridas. La columna "consumo" es la diferencia con `con_trabajadores`, medida antes de que el padre espere a los hijos.

| K | Páginas libres con trabajadores | Consumo medido con CoW | Consumo estimado sin CoW |
|---|---|---|---|
| 1 | 32 424 | 9 | ≈ 77 |
| 2 | 32 415 | 18 | ≈ 154 |
| 3 | 32 406 | 27 | ≈ 231 |
| 4 | 32 397 | 36 | ≈ 308 |
| 8 | 32 361 | 72 | ≈ 616 |

El consumo sin CoW se estima así: cada hijo copiaría unas 68 páginas de usuario (64 de la tabla de cuentas y 4 de texto, datos, guarda y pila), además de sus 9 páginas de estructura. Con CoW, esas 68 páginas se comparten y el hijo solo consume estructura.

En el snapshot (`minidb snapshot 2000`), el hijo consume 9 páginas de estructura. Durante las transferencias ocurrieron 30 fallos CoW y 30 copias, una por cada página tocada. Sin CoW, el `fork()` copiaría unas 68 páginas antes de que el padre aplicara la primera transferencia.

## 3. Decisiones de diseño, limitaciones y conclusiones

**Decisiones.** `refs` tiene su propio lock para no bloquear la lista libre. `freerange()` pone el conteo en 1 antes de liberar, porque las páginas libres nunca pasaron por `kalloc()`. `cowfault()` reserva la copia antes de tocar la PTE, así que con falta de memoria la tabla queda intacta. `copyout()` lee la dirección física después de resolver CoW. `sfence_vma()` sigue a cada cambio de PTE, incluido el camino de error de `uvmcopy()`. La versión de `vmprint()` es la del Proyecto 3, porque devuelve `int`.

**Limitaciones.** `pgaccess()` limpia `PTE_A` antes de copiar el resultado: si la dirección del resultado es inválida, la información se pierde. `cowstat()` cuenta las páginas libres recorriendo la lista, con costo O(n). `refs.count` ocupa 128 KB dentro del kernel. El camino de error de `uvmcopy()` no lo ejercen las pruebas del docente, porque necesita quedarse sin memoria a mitad del recorrido. `cowfault()` no es atómico: el conteo puede bajar entre la lectura y la copia. En ese caso se hace una copia de más, nunca se pierde un dato.

**Conclusiones.** Con CoW, `fork()` no copia las páginas de datos: el costo de un hijo es su estructura, unas 9 páginas, frente a unas 77 sin CoW. En la consulta, con 3 o más trabajadores y 3 CPUs, el tiempo baja de 19 a 7 ticks. Pasar de 3 a 8 trabajadores no mejora el resultado porque las tres CPUs ya están ocupadas.
