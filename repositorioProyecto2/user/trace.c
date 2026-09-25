#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Uso: trace mask comando [argumentos...]\n");
    exit(1);
  }

  // Inicializar todo el arreglo a ceros/NULL
  memset(nargv, 0, sizeof(nargv));

  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "trace: fallo en activar el rastreo\n");
    exit(1);
  }
  
  // Copiar el comando y sus argumentos
  for(i = 2; i < argc; i++){
    nargv[i-2] = argv[i];
  }

  exec(nargv[0], nargv);
  
  // Si exec falla, reportar error explícito
  fprintf(2, "trace: exec %s fallo\n", nargv[0]);
  exit(1);
}
