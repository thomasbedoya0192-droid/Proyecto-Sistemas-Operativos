#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int getcmd(char *buf, int nbuf) {
  fprintf(2, "mi_shell$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0)
    return -1;
  return 0;
}

int main(void) {
  static char buf[100];
  int fd;

  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  while(getcmd(buf, sizeof(buf)) >= 0){
    int len = strlen(buf);
    if(len > 0 && buf[len-1] == '\n') {
        buf[len-1] = '\0';
    }

    if(strlen(buf) == 0) {
        continue;
    }

    if(strcmp(buf, "exit") == 0){
      break; 
    }

    // --- NUEVA LÓGICA DE LA FASE 2 ---

    // 1. ANÁLISIS (PARSING): Separar el texto en argumentos
    char *argv[100];
    int argc = 0;
    char *token = buf;

    while(*token != '\0' && argc < 99) {
        // Ignorar espacios en blanco
        while(*token == ' ' || *token == '\t') {
            *token = '\0'; // Cortamos la cadena aquí
            token++;
        }
        if(*token == '\0') break;

        argv[argc++] = token; // Guardamos el inicio de la palabra

        // Avanzar hasta el final de la palabra
        while(*token != ' ' && *token != '\t' && *token != '\0') {
            token++;
        }
    }
    argv[argc] = 0; // exec() exige que el último elemento del arreglo sea null

    if(argc == 0) continue; // Si solo escribieron espacios, volvemos a preguntar

    // 2. EJECUCIÓN: Crear proceso y ejecutar comando
    int pid = fork();

    if(pid < 0){
      fprintf(2, "Error: fork falló\n");
    } 
    else if(pid == 0){
      // --- CÓDIGO DEL PROCESO HIJO ---
      exec(argv[0], argv);
      
      // Si exec() funciona, el programa hijo se transforma y esta línea NUNCA se ejecuta.
      // Si llegamos aquí, es porque exec() falló (ej. el comando no existe).
      fprintf(2, "Error: comando '%s' no encontrado\n", argv[0]);
      exit(1);
    } 
    else {
      // --- CÓDIGO DEL PROCESO PADRE (El Shell) ---
      wait(0); // El shell espera pacientemente a que el hijo termine
    }
  }
  
  exit(0);
}
