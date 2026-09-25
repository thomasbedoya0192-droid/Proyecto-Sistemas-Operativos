# Implementación de System Calls en xv6-riscv

## Descripción General

Este proyecto implementa dos nuevas llamadas al sistema en xv6-riscv para agregar capacidades de introspección y monitoreo del kernel:

- **`trace`**: Rastrea llamadas al sistema ejecutadas por un proceso y sus hijos, imprimiendo qué syscalls se invocaron y sus valores de retorno.
- **`sysinfo`**: Consulta información actual del sistema: memoria libre, páginas usadas, páginas disponibles y procesos listos para ejecutar.

## Integrantes

- Thomas Bedoya Rendón
- David Guerra Morales
- Eder Ceballos Quiroz

---

## Cómo Compilar

```bash
make clean
make
```

## Cómo Ejecutar

Iniciar QEMU con xv6:

```bash
make qemu
```

---

## Usar `trace`

`trace` permite filtrar y rastrear syscalls específicas usando una máscara de bits. El número de la máscara es $2^{\text{SYS\_id}}$ para cada syscall.

### Ejemplos

Rastrear `read` (SYS_read = 5, máscara = 32):
```
$ trace 32 grep hello README
```

Rastrear `fork` (SYS_fork = 1, máscara = 2) y `exit` (SYS_exit = 2, máscara = 4), total = 6:
```
$ trace 6 sh
```

La salida muestra cada syscall interceptada con su nombre y valor de retorno.

---

## Usar `sysinfo`

`sysinfo` imprime el estado actual de los recursos del sistema.

```
$ sysinfo
```

**Salida típica:**

```
Free Memory: 127 MB
Used Pages: 232
Available Pages: 32536
Runnable Processes: 0
```

---

## Cambios Realizados

### Archivos Creados

| Archivo | Descripción |
|---------|-------------|
| `kernel/sysinfo.h` | Define `struct sysinfo` con campos de memoria y procesos |
| `kernel/sysinfo.c` | Implementa `systeminfo()` para recopilar datos y copiarlos a usuario |
| `user/trace.c` | Programa CLI para invocar trace y ejecutar comandos |
| `user/sysinfo.c` | Programa CLI para invocar sysinfo e imprimir resultados |

### Archivos Modificados

**Kernel:**
- `kernel/syscall.h`: Agrega `SYS_trace 22` y `SYS_sysinfo 23`
- `kernel/syscall.c`: Registra handlers y modifica `syscall()` para interceptar basado en `tracemask`
- `kernel/sysproc.c`: Implementa `sys_trace()` y `sys_sysinfo()`
- `kernel/proc.h`: Agrega campo `int tracemask` a `struct proc`
- `kernel/proc.c`: Modifica `fork()` para heredar `tracemask` e implementa `count_runnable()`
- `kernel/kalloc.c`: Implementa `kmeminfo()` para calcular memoria
- `kernel/defs.h`: Agrega prototipos globales

**Usuario:**
- `user/user.h`: Declara prototipos `trace()` e `sysinfo()`
- `user/usys.pl`: Agrega entry points para generar stubs en ensamblador
- `Makefile`: Registra nuevos objetos y programas

---

## Detalles Técnicos

### `trace`

La máscara de bits funciona así: si queremos rastrear la syscall con número `n`, setemos el bit `n` en la máscara. En tiempo de ejecución, para cada syscall con número `num`, verificamos:

```c
if ((p->tracemask >> num) & 1) {
  // Esta syscall se rastrea
}
```

El `tracemask` se hereda de padre a hijo en `fork()`, así que si el padre está rastreando, sus hijos también lo harán.

### `sysinfo`

Recopila datos de dos fuentes principales:

1. **Memoria**: Itera sobre `kmem.freelist` (lista de páginas libres) bajo el candado `kmem.lock`:
   - Páginas libres = cantidad de nodos en freelist
   - Páginas usadas = total - libres
   - MB libres = (páginas_libres × 4096) / (1024 × 1024)

2. **Procesos**: Recorre el array `proc[]` contando procesos con `state == RUNNABLE` bajo su candado individual.

Los datos se copian al espacio de usuario de forma segura mediante `copyout()`, que valida la dirección del usuario antes de escribir.

---

## Estructura de `sysinfo`

```c
struct sysinfo {
  uint64 freemem_mb;      // Memoria libre en MB
  uint64 used_pages;      // Páginas de 4KB usadas
  uint64 avail_pages;     // Páginas de 4KB disponibles
  uint64 runnable_proc;   // Procesos en estado RUNNABLE
};
```

---

## Notas Importantes

- **trace**: La máscara persiste a través de `exec()` porque es parte de la estructura `proc`, que mantiene sus campos administrativos después de reemplazar el código/datos.
- **sysinfo**: Los valores son un "snapshot" del momento de la llamada. Cambios concurrentes pueden causar pequeñas discrepancias, pero el mecanismo es seguro gracias a los candados del kernel.
- Ambas syscalls retornan 0 en éxito, -1 en error (ej: dirección inválida en sysinfo).

---

## Validación

Las implementaciones fueron probadas en QEMU y validadas manualmente:

- ✓ `trace` rastrea correctamente syscalls seleccionadas
- ✓ `trace` hereda la máscara en procesos hijos
- ✓ `sysinfo` imprime valores razonables
- ✓ No hay kernel panics con entrada inválida
- ✓ Compilación sin errores o warnings

---

## Declaración de Uso de IA

Se utilizaron herramientas de Inteligencia Artificial Generativa (Gemini y Claude) durante el desarrollo del proyecto para:

- Consultas sobre estructuras internas del kernel xv6-riscv
- Clarificación de conceptos sobre RISC-V y syscalls
- Asistencia en debugging de errores de compilación
- Estructura y redacción de documentación técnica

Todo el código implementado fue revisado, probado y validado manualmente por los integrantes del equipo antes de ser incluido en el proyecto final.
