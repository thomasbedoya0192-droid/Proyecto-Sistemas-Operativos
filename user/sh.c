#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Función auxiliar para leer una línea de la consola
int getcmd(char *buf, int nbuf) {
  fprintf(2, "mi_shell$ "); // Nuestro prompt personalizado
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF (Ctrl+D)
    return -1;
  return 0;
}

int main(void) {
  static char buf[100];

  // Asegurarnos de que los descriptores 0, 1 y 2 están abiertos.
  int fd;
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Bucle principal del shell
  while(getcmd(buf, sizeof(buf)) >= 0){
    
    // Eliminar el salto de línea al final (si existe)
    int len = strlen(buf);
    if(len > 0 && buf[len-1] == '\n') {
        buf[len-1] = '\0';
    }

    // Si el usuario no escribió nada, volvemos a preguntar
    if(strlen(buf) == 0) {
        continue;
    }

    // 1. Comando interno obligatorio: exit
    if(strcmp(buf, "exit") == 0){
      break; 
    }

    // 2. Aquí irá la lógica para ejecutar comandos (ls, echo, etc.)
    // Por ahora, solo haremos un "echo" de prueba para confirmar que lee bien.
    printf("Comando ingresado: %s\n", buf);
  }
  
  // Salida controlada del shell
  exit(0);
}
