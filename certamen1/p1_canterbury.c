#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define BUF_SIZE 256

int main() {

    // Inicializar la semilla aleatoria 
    srand((unsigned int)time(NULL));

    int pipe_fd[2]; //0 read y 1 write

    if (pipe(pipe_fd) == -1){
        perror("pipe");
        exit(EXIT_FAILURE);
        return 0;
    }

    // Creamos 2 procesos?
    pid_t pid = fork();

    // El padre lee, pid > 0
    if (pid > 0) {
        // Cerramos la escritura ya que no la usaremos
        close(pipe_fd[1]);
        
        // Variable para almacenar lo que vamos a leer
        char buffer[BUF_SIZE];
        ssize_t n;

        int cont_norm = 0;
        int cont_crit = 0;
        int cont_elev = 0;

        while ((n = read(pipe_fd[0], buffer, sizeof(buffer)-1)) > 0){

            if (n == -1){
                perror("read");
                exit(EXIT_FAILURE);
            }

            // Agregamos caracter de finalizacion para no leer los 256 bytes innecesariamente
            buffer[n] = '\0';

            printf("[PUENTE] RAD -> %s", buffer);

            // Logica para analizar las entradas
            // Variable para guardar la radiación
            float rad;

            // Almacenamos el valor recibido en la variable rad
            sscanf(buffer, "[SENSOR] Enviando RAD %f", &rad);

            // Interpretamos el valor
            if (rad < 3.0){
                cont_norm++;
                printf("NORMAL\n");
            }
            else if (rad < 6.0){
                cont_elev++;
                printf("ELEVADO\n");
            }
            else {
                cont_crit++;
                printf("CRITICO\n");
            }

        }

        // Termina la lectura entonces la cerramos
        close(pipe_fd[0]);
        // Esperamos al hijo
        wait(NULL);
        printf("[PUENTE] Resumen: NORMAL=%d ELEVADO=%d CRITICO=%d\n", cont_norm, cont_elev, cont_crit);
    }

    // Proceso hijo 
    else {
        // Cerramos la lectura ya que no la vamos a usar
        close(pipe_fd[0]);
        // Mensaje a enviar
        char msg[BUF_SIZE];
        
        // Bucle para enviar
        for (int i = 0; i < 10; i++){
            // Generamos el numero
            float resultado = (float)rand() / RAND_MAX * 10.0;
            // Formateamos el mensaje
            snprintf(msg, sizeof(msg), "[SENSOR] Enviando RAD %.2f\n", resultado);
            // Escribimos el mensaje
            write(pipe_fd[1], msg, strlen(msg));
            // Esperamos 1 segundo antes de mandar la siguiente señal
            sleep(1);
        }
        
        // Cerramos la escritura
        close(pipe_fd[1]);
    }

    return 0;
}