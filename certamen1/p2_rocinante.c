#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define BUF_SIZE 256

int main (){

    // Inicializar la semilla aleatoria
    srand((unsigned int)time(NULL));

    // ---------- 1. Creamos los 2 pipes ANTES de cualquier fork ----------
    // Así los 3 procesos heredan los 4 extremos y cada uno cierra lo que no usa

    // pipe entre sensor (A) y filtro (B)
    int pipe_1[2];

    if (pipe(pipe_1) == -1){
        perror("pipe");
        return 1;
    }

    // pipe entre filtro (B) y puente (C)
    int pipe_2[2];

    if (pipe(pipe_2) == -1){
        perror("pipe");
        return 1;
    }

    // ---------- 2. Primer fork: nace el proceso B (filtro) ----------
    pid_t pid1 = fork();

    if (pid1 == -1){
        perror("fork");
        return 1;
    }

    // Proceso B: lee de pipe_1 y escribe en pipe_2
    if (pid1 == 0){
        // Cerramos lo que B no usa: escritura de pipe_1 y lectura de pipe_2
        close(pipe_1[1]);
        close(pipe_2[0]);

        // Variables a recibir y enviar
        char msg[BUF_SIZE];
        char buf2[BUF_SIZE];
        int temp;

        // Bytes que vamos a recibir
        ssize_t n;

        // Mientras A siga enviando (read devuelve 0 cuando A cierra pipe_1)
        while((n = read(pipe_1[0], msg, sizeof(msg) - 1)) > 0){

            // Ponemos caracter final
            msg[n] = '\0';
            // Sacamos la información del mensaje
            sscanf(msg, "TEMP %d", &temp);

            // Si la temperatura es válida la mandamos a C
            if (temp >= 20 && temp <= 200){
                // Le damos el formato
                snprintf(buf2, sizeof(buf2), "TEMP_OK %d\n", temp);
                // Mandamos el mensaje
                write(pipe_2[1], buf2, strlen(buf2));
            }

            // Si la temperatura no está en el rango
            else {
                printf("[FILTRO] Descartado: %d°C\n", temp);
            }
        }

        // Cerramos lo que usamos. Al cerrar pipe_2[1], el read de C devuelve 0
        close(pipe_1[0]);
        close(pipe_2[1]);
        // B termina aquí y NO sigue ejecutando el resto del main
        exit(0);
    }

    // ---------- 3. Segundo fork: solo llega aquí el padre (A) ----------
    // B ya hizo exit(0) arriba, así que este fork lo ejecuta un solo proceso
    pid_t pid2 = fork();

    if (pid2 == -1){
        perror("fork");
        return 1;
    }

    // Proceso C: lee de pipe_2 e imprime
    if (pid2 == 0){
        // Cerramos lo que C no usa: pipe_1 completo y escritura de pipe_2
        close(pipe_1[0]);
        close(pipe_1[1]);
        close(pipe_2[1]);

        // Variables que vamos a recibir
        char msgB[BUF_SIZE];
        int tempOp;

        // Numero de bytes del segundo mensaje
        ssize_t n2;

        // Mientras B siga enviando (read devuelve 0 cuando B cierra pipe_2)
        while ((n2 = read(pipe_2[0], msgB, sizeof(msgB) - 1)) > 0){
            // Le ponemos caracter final
            msgB[n2] = '\0';
            // Sacamos la información del mensaje
            sscanf(msgB, "TEMP_OK %d", &tempOp);
            // Imprimimos el valor operacional
            printf("[PUENTE] Temperatura operacional: %d°C\n", tempOp);
        }

        // Cerramos la lectura
        close(pipe_2[0]);
        // C termina aquí
        exit(0);
    }

    // ---------- 4. Proceso A (padre): el sensor ----------
    // Cerramos lo que A no usa: lectura de pipe_1 y pipe_2 completo
    close(pipe_1[0]);
    close(pipe_2[0]);
    close(pipe_2[1]);

    // Variables a enviar
    char buf[BUF_SIZE];
    // Rango de temperaturas del sensor
    int min = -50, max = 300;

    // 15 lecturas aleatorias
    for (int i = 0; i < 15; i++){
        // Generamos la temperatura entre -50 y 300
        int valor = (rand() % (max - min + 1)) + min;
        // Le damos formato al mensaje
        snprintf(buf, sizeof(buf), "TEMP %d\n", valor);
        // Mandamos el mensaje
        write(pipe_1[1], buf, strlen(buf));
        // Igual que en P1: la pausa hace que cada read de B reciba un solo mensaje
        sleep(1);
    }

    // Cerramos la escritura. El read de B devuelve 0 y B termina
    close(pipe_1[1]);

    // Esperamos a los 2 hijos (B y C): un wait por cada hijo
    wait(NULL);
    wait(NULL);

    return 0;
}
