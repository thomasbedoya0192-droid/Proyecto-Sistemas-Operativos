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
    
    int pid = fork();
    if(pid<0){
      printf("error al crear el hijo");
    }

    if(pid==0){
	    //parser
	    int i =0;
	    int argc = 0; //contador de palabras
      int word = 0; 
    	char *argv[10];
    
      while(buf[i] != '\0'){
	      if(buf[i]==' ' || buf[i]=='\n'){
	        buf[i]='\0';
	        word=0;
	      }else{
	        if(word==0 && argc<9){
			      argv[argc]=&buf[i];
			      argc= argc+1;
			      word=1;
	   	    }
	      }
	      i=i+1;
      }
      argv[argc]=0; // el ultimo elemento del arreglo debe ser 0

 	    exec(argv[0],argv);
      
      //esta linea jamas se ejecutara si el exec tiene exito porque se transformara en otro programa
	    printf("fallo de ejecucion");
	    exit(1);
    }else{
	    wait(0);
    }
	
  
  // Salida controlada del shell
  exit(0);
}
}