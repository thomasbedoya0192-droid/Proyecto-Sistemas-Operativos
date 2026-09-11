#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int getcmd(char *buf, int nbuf)
{
  fprintf(2, "mi_shell$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);

  if (buf[0] == 0)
    return -1;

  return 0;
}

int main(void)
{
  static char buf[100];
  int fd;

  // Asegurar stdin, stdout, stderr
  while ((fd = open("console", O_RDWR)) >= 0)
  {
    if (fd >= 3)
    {
      close(fd);
      break;
    }
  }

  while (getcmd(buf, sizeof(buf)) >= 0)
  {
    int len = strlen(buf);

    if (len > 0 && buf[len - 1] == '\n')
      buf[len - 1] = '\0';

    if (strlen(buf) == 0)
      continue;

    if (strcmp(buf, "exit") == 0){
      exit(0);}

    // -------- Pipes --------
    char *comandos[10];
    int num_cmd = 0;
    comandos[num_cmd++] = buf;

    for (int i = 0; buf[i] != '\0'; i++)
    {
      if (buf[i] == '|')
      {
        buf[i] = '\0';
        comandos[num_cmd++] = &buf[i + 1];
      }
    }

    int fd_in = 0;
    int p[2];

    for (int i = 0; i < num_cmd; i++)
    {
      if (i < num_cmd - 1)
        pipe(p);

      int pid = fork();

      if (pid < 0)
      {
        fprintf(2, "Error: fork falló\n");
      }
      else if (pid == 0)
      {
        // ===== HIJO =====

        // Redirección de entrada
        if (fd_in != 0)
        {
          close(0);
          dup(fd_in);
          close(fd_in);
        }

        // Redirección de salida
        if (i < num_cmd - 1)
        {
          close(1);
          dup(p[1]);
          close(p[0]);
          close(p[1]);
        }

        // Copia local del comando
        char cmd_copy[100];
        int c_idx = 0;

        while (comandos[i][c_idx] != '\0' && c_idx < 99)
        {
          cmd_copy[c_idx] = comandos[i][c_idx];
          c_idx++;
        }
        cmd_copy[c_idx] = '\0';

        // -------- Parser --------
        int j = 0;
        int argc = 0;
        int inWord = 0;
        char *argv[10];

        char type_redir = 0;
        char *file = 0;
        char *cmd_str = cmd_copy;

        while (cmd_str[j] != '\0')
        {
          if (cmd_str[j] == ' ' || cmd_str[j] == '\t' || cmd_str[j] == '\n')
          {
            cmd_str[j] = '\0';
            inWord = 0;
          }
          else if (cmd_str[j] == '<' || cmd_str[j] == '>')
          {
            type_redir = cmd_str[j];
            cmd_str[j] = '\0';
            inWord = 0;
          }
          else
          {
            if (!inWord)
            {
              if (type_redir == 0 && argc < 9)
              {
                argv[argc++] = &cmd_str[j];
              }
              else if (type_redir != 0 && file == 0)
              {
                file = &cmd_str[j];
              }
              inWord = 1;
            }
          }
          j++;
        }

        argv[argc] = 0;

        if (argc == 0)
          exit(0);

        // -------- Redirecciones --------
        if (type_redir == '>')
        {
          close(1);
          if (open(file, O_WRONLY | O_CREATE) < 0)
          {
            fprintf(2, "Error: no se pudo crear %s\n", file);
            exit(1);
          }
        }
        else if (type_redir == '<')
        {
          close(0);
          if (open(file, O_RDONLY) < 0)
          {
            fprintf(2, "Error: no se pudo abrir %s\n", file);
            exit(1);
          }
        }

        // -------- Ejecutar --------
        exec(argv[0], argv);

        fprintf(2, "Error: comando '%s' no encontrado\n", argv[0]);
        exit(1);
      }
      else
      {
        // ===== PADRE =====

        if (fd_in != 0)
          close(fd_in);

        if (i < num_cmd - 1)
        {
          close(p[1]);
          fd_in = p[0];
        }
      }
    }

    for (int i = 0; i < num_cmd; i++)
      wait(0);
  }

  exit(0);
}