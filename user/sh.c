#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Función auxiliar para leer una línea de la consola
int getcmd(char *buf, int nbuf)
{
  fprintf(2, "mi_shell$ "); // Nuestro prompt personalizado
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if (buf[0] == 0) // EOF (Ctrl+D)
    return -1;
  return 0;
}

int main(void)
{
  static char buf[100];

  // Asegurarnos de que los descriptores 0, 1 y 2 están abiertos.
  int fd;
  while ((fd = open("console", O_RDWR)) >= 0)
  {
    if (fd >= 3)
    {
      close(fd);
      break;
    }
  }

  // Bucle principal del shell
  while (getcmd(buf, sizeof(buf)) >= 0)
  {

    // Eliminar el salto de línea al final (si existe)
    int len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
    {
      buf[len - 1] = '\0';
    }

    // Si el usuario no escribió nada, volvemos a preguntar
    if (strlen(buf) == 0)
    {
      continue;
    }

    // 1. Comando interno obligatorio: exit
    if (strcmp(buf, "exit") == 0)
    {
      break;
    }

    // 2. Aquí irá la lógica para ejecutar comandos (ls, echo, etc.)
    // Por ahora, solo haremos un "echo" de prueba para confirmar que lee bien.

    int pid = fork();
    if (pid < 0)
    {
      printf("error al crear el hijo");
    }

    if (pid == 0)
    {
      // parser
      int i = 0;
      int argc = 0; // contador de palabras
      int inWord = 0;
      char *argv[10];
      char type_redir = 0;
      char *file = 0; //verificar si no hay un archivo capturado en ese momento

      while (buf[i] != '\0')
      {
        if (buf[i] == ' ' || buf[i] == '\n')
        {
          buf[i] = '\0';
          inWord = 0;
        }
        else if (buf[i] == '<' || buf[i] == '>')
        {
          type_redir = buf[i];
          buf[i] = '\0';
          inWord = 0;
        }
        else
        {
          if (inWord == 0)
          {
            if (type_redir == 0 && argc < 9)
            {
              argv[argc] = &buf[i];
              argc = argc + 1;
            }
            else if (type_redir != 0 && file == 0)
            {
              file = &buf[i];
            }
            inWord = 1;
          }
        }
        i = i + 1;
      }
      argv[argc] = 0; // el ultimo elemento del arreglo debe ser 0

      // cambio de descriptores
      if (type_redir == '>')
      {
        close(1); // cerramos la pantalla
        // abrimos fille como descriptor para que el hijo no mande todo a la pantalla sino al archivo
        if (open(file, O_WRONLY | O_CREATE) < 0)
        {
          printf("error al abrir el archivo %s\n", file);
          exit(1);
        }
      }
      else if (type_redir == '<')
      {
        close(0); // cerramos teclado
        if (open(file, O_RDONLY) < 0)
        {
          printf("error al abrir %s\n", file);
          exit(1);
        }
      }
      exec(argv[0], argv);

      printf("fallo de ejecucion");
      exit(1);
    }
    else
    {
      wait(0);
    }

    // Salida controlada del shell
    exit(0);
  }
}