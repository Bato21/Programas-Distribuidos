/*
 * SIMULACRO CERTAMEN - ESTACIÓN SISMOLÓGICA (pipes + cliente TCP)   -- esqueleto
 *
 *   SISMOGRAFO (hijo 1) --pipe1--> FILTRO (hijo 2) --pipe2--> TRANSMISOR (padre) --TCP--> CENTRAL
 *
 * Compilar: gcc -Wall -o estacion estacion.c
 * Ejecutar: ./estacion VILLARRICA      (con la central ya corriendo)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT       7000
#define IP         "127.0.0.1"
#define BUF_SIZE   256
#define N_LECTURAS 12

/* ---------- DADO: no se modifica ---------- */
char *checksum(const char *valor, char out[3]) {
    unsigned int s = 0;
    for (int i = 0; valor[i]; i++) s += (unsigned char)valor[i];
    snprintf(out, 3, "%02X", s % 256);
    return out;
}
/* ------------------------------------------ */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <VOLCAN>\n", argv[0]);
        return 1;
    }
    const char *volcan = argv[1];

    /* TODO B.1: pipes y forks */

    // Pipe sensor a filtro
    int pipe_1[2];
    // pipe filtro a estacion
    int pipe_2[2];

    // Si falla el pipe mandamos alerta
    if (pipe(pipe_1) < 0 || pipe(pipe_2)){
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Son 3 procesos

    // Abrimos primero el proceso sensor filtro

    pid_t pid1 = fork();

    // Si falla el fork
    if (pid1 < 0){
        perror("fork");
        exit(EXIT_FAILURE);
    }


    /* TODO B.2: SISMOGRAFO (hijo 1) */

    // Proceso hijo es decir el sensor (sismografo)
    if (pid1 == 0){
        // Cerramos lo que no usamos
        close(pipe_1[0]);
        // Escribir del segundo pipe que no usamos
        close(pipe_2[1]);
        // Escuchar del segundo pipe que no usamos
        close(pipe_2[0]);

        // SOLO LE ESCRIBO AL FILTRO
        // Declaro lo que mando

        char buf[BUF_SIZE];
        int seq;
        float mag;

        // Con & porque no tienen valor asignado aún, sin inicializar
        snprintf(buf, sizeof(buf), "LECT %d %f\n", &seq, &mag);
        // Mandamos el mensaje
        write(pipe_1[1], buf, strlen(buf));
        // Dejamos de escribir
        close(pipe_1[1]);
        // MATAMOS EL PROCESO HIJO DEL SISMOGRAFO PARA QUE SE FORKEE
        // Pasamos de 2 proceso a 1 solo
        exit(EXIT_SUCCESS);
    }

    /* TODO B.3: FILTRO (hijo 2) */

    // Forkeamos de nuevo ahora para filtro <-> Transmisor
    pid_t pid2 = fork();

    if (pid2 < 0){
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // El segundo hijo (filtro)
    if(pid2 == 0){
        // Cierro lo que no uso
        close(pipe_1[1]);
        close(pipe_2[0]);

        // Declaramos lo que recibimos del pipe 1
        char msg[BUF_SIZE];
        int seq;
        float mag;
        // Bytes que recibimos
        ssize_t n;

        // Declaramos lo que mandamos por elpipe 2
        char buf2[BUF_SIZE];
        int seq2;
        float mag2;

        // Bucle para recibir mensajes
        // Mientras n = bytes recibidos sea mayor a 0
        // Le asignamos valor a n en el while
        while((n = read(pipe_1[0], msg, sizeof(msg) - 1)) > 0){
            // Si hay error
            if (n < 0){
                perror("read");
                exit(EXIT_FAILURE);
            }

            // Proceso de filtro
            // Le ponemos termino de fin al mensaje
            msg[n] = '\0';
            
            // Sacamos variables
            sscanf(msg, "LECT %d %.2f", &seq2, &mag2);

            // Filtro a comparar

            if (seq2 < 0){
                // Se elimina y exit
            }

            // Chequear que volcan es i medir su magnitud

            if (seq != 1){
                if (mag2 < 2) {

                }
                else if (mag2 < 4){

                }
                else {
                    // Si no pasa el filtro escribimos en terminañ
                    printf("Medicion no valida");
                }
            }


            // Cuando el mensaje pasa el filtro lo mandamos a la central
            snprintf(buf2, sizeof(buf2), "VALIDA %d %f\n", seq2, mag2);
            write(pipe_2[1], buf2, strlen(buf2));
        }

        // Cerramos todo y matamos el proceso
        close(pipe_1[0]);
        close(pipe_2[1]);
        exit(EXIT_SUCCESS);
    }


    /* TODO B.4: TRANSMISOR (padre) */

    // Proceso padre
    else{
        // CIerro todo excepto pipe_2[0]
        
        // Variables que recibo

        // Checkeo formato

        // Imprimo resultado

        // Cierro todo

    }

    return 0;
}
