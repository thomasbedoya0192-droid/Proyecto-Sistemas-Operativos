# Proyecto de Sistemas Operativos
## Implementación de un Shell para xv6

### Integrantes
* Thomas Bedoya Rendon
* David Guerra Morales
* Eder Ceballos Quiroz

### Curso
Sistemas Operativos

### Semestre
2026-2

# Descripción
Este proyecto implementa un shell para el sistema operativo xv6. El programa permite interpretar comandos ingresados por el usuario desde la entrada estándar y ejecutar los programas existentes en xv6 mediante la creación de nuevos procesos.

La implementación soporta:
- Ejecución de comandos simples.
- Ejecución de comandos con argumentos.
- Redirección de entrada (`<`).
- Redirección de salida (`>`).
- Tuberías simples y múltiples (`|`).
- Comando interno `exit` para finalizar el shell.

La solución fue desarrollada utilizando lenguaje C y se integra al proceso de compilación estándar de xv6 ejecutado sobre QEMU.

# Estructura de archivos
```text
user/
  sh.c
