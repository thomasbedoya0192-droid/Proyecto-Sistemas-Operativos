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

        int pid = fork();

        if(pid < 0){
            fprintf(2, "Error: fork falló\n");
        } 
        else if(pid == 0){//codigo del hijo
            
            // parser
            int i = 0;
            int argc = 0;
            int inWord = 0;
            char *argv[10];
            char type_redir = 0;
            char *file = 0;

            while(buf[i] != '\0') {
                if(buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n') {
                    buf[i] = '\0';
                    inWord = 0;
                } else if(buf[i] == '<' || buf[i] == '>') {
                    type_redir = buf[i];
                    buf[i] = '\0';
                    inWord = 0;
                } else {
                    if(inWord == 0) {
                        if(type_redir == 0 && argc < 9) {
                            argv[argc] = &buf[i];
                            argc++;
                        } else if(type_redir != 0 && file == 0) {
                            file = &buf[i];
                        }
                        inWord = 1;
                    }
                }
                i++;
            }
            argv[argc] = 0;

            if(argc == 0) exit(0);

            // redirecciones
            if(type_redir == '>') {
                close(1);
                if(open(file, O_WRONLY | O_CREATE) < 0) {
                    fprintf(2, "Error: no se pudo abrir/crear %s\n", file);
                    exit(1);
                }
            } else if(type_redir == '<') {
                close(0);
                if(open(file, O_RDONLY) < 0) {
                    fprintf(2, "Error: no se pudo abrir %s\n", file);
                    exit(1);
                }
            }

            // ejecucion del hijo
            exec(argv[0], argv);
            
            fprintf(2, "Error: comando '%s' no encontrado\n", argv[0]);
            exit(1);
        } 
        else {
            //codigo del padre
            wait(0);
        }
    }
    
    exit(0);
}
