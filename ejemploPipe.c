#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int pipe_fd[2]; // [0]=read, [1]=write
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Proceso hijo
        close(pipe_fd[1]); //Cierra la escritura

        char buffer[256]; // Buffer para leer del pipe
        ssize_t n = read(pipe_fd[0], buffer, sizeof(buffer) - 1); // Lee del pipe
        if (n == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        buffer[n] = '\0'; // Agrega caracter de termino para dejar de leer
        printf("Hijo recibió: %s\n", buffer); // Imprime lo recibido

        close(pipe_fd[0]); //Cierra la lectura
    } else {
        // Proceso padre
        close(pipe_fd[0]); //Cierra la lectura

        char *msg = "Hola desde el padre!"; // Mensaje a enviar
        write(pipe_fd[1], msg, strlen(msg)); // Escribe en el pipe

        close(pipe_fd[1]); //Cierra la escritura
        wait(NULL); // Espera a que el hijo termine
    }

    return 0;
}
