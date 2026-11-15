/*
 * Simulacro Control 03 — Base CÓNDOR (Parte A)
 *
 * Compilar:  gcc -Wall -Wextra -g -o base base.c
 * Ejecutar:  ./base
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        7300
#define BUF_SIZE    256
#define N_DRONES    5
#define QUORUM      (N_DRONES / 2 + 1)     /* 3 */
#define TIMEOUT_HB  3                      /* segundos sin mensajes => CAIDO */

/* ---------- DADO: lo que viaja por la pipe hijo -> padre ---------- */
#define T_HB   1
#define T_EVT  2
#define T_BYE  3

typedef struct {
    int  id;          /* 1..N_DRONES                      */
    int  tipo;        /* T_HB, T_EVT o T_BYE              */
    int  seq;         /* solo EVT                         */
    int  T;           /* timestamp Lamport del mensaje    */
    char texto[128];  /* solo EVT                         */
} Aviso;

/* ---------- DADO: estado de cada dron, lo mantiene SOLO el padre ---------- */
#define LIBRE   0     /* nunca se conectó o se fue (BYE) */
#define VIVO    1
#define CAIDO   2

typedef struct {
    int    fd;          /* socket del dron (para que el padre responda) */
    int    estado;      /* LIBRE, VIVO o CAIDO                          */
    time_t ultimo_hb;   /* última vez que se supo de él                 */
    int    ultimo_seq;  /* último EVT aplicado al log (0 = ninguno)     */
} Dron;

Dron drones[N_DRONES + 1];   /* índice 1..N_DRONES (el 0 no se usa) */
int  reloj = 0;              /* reloj de Lamport de la base */

/* ---------- DADO ---------- */
int max(int a, int b) { return a > b ? a : b; }

int contar_vivos(void) {
    int v = 0;
    for (int i = 1; i <= N_DRONES; i++)
        if (drones[i].estado == VIVO) v++;
    return v;
}

/*
 * Hijo "cartero": lee el socket de UN dron y le pasa cada mensaje al padre
 * por la pipe, como un Aviso. No responde nada al dron.
 */
void atender_dron(int fd, int id, int pipe_w) {
    char buf[BUF_SIZE];
    Aviso a;

    while (1) {
        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';

        /* DADO: un recv() puede traer varias líneas pegadas (HB + EVT).
         * Este bucle entrega una línea a la vez en 'linea', ya sin '\n'. */
        char *save = NULL;
        for (char *linea = strtok_r(buf, "\n", &save); linea; linea = strtok_r(NULL, "\n", &save)) {

            /* TODO A.2 */

        }
    }

    /* TODO A.2 (fin) */
}

int main(void) {

    /* TODO A.1 */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0){ perror("socket"); return 1;}

    // Reusar puerto inmediatamente
    // Solo si lo piden
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Creamos estructura
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY //Solo server
    };

    // En cliente pones inet_pton(client_fd, ip, &addr.s_addr);

    if (bind(server_fd, (struct sockaddr_in *)&addr, sizeof(addr)) < 0){
        perror("bind");
        return 1;
    }

    if (listen(server_fd, N_DRONES) < 0){
        perror("listen");
        return 1;
    }


    while(1){
        int pipe_fd[2];

        if(pipe(pipe_fd) < 0){
            perror("pipe");
            return 1;
        }

        // Set de cosas a revisar
        fd_set set;
        // Vaciamos el set de cosas a revisar
        FD_ZERO(&set);
        // Agregamos tanto el pipe como el server
        FD_SET(server_fd, &set);
        FD_SET(pipe_fd[0], &set);
        int max_fd = max(pipe_fd[0], server_fd);
        struct timeval tv = {1, 0}; // tv es 1 segundo
        // Me pongo a revisar si llega algo
        // Reviso de 0 a max_fd + 1
        // El set que voy a revisar
        // NULL NULL
        // Cuanto tiempo espero antes de timeout -> &tv
        select(max_fd + 1, &set, NULL, NULL, &tv);

        // Si llega algo del servidor:
        if(FD_ISSET(server_fd, &set)){

            int client_fd = accept(server_fd, NULL, NULL);



            pid_t pid = fork();
            if(pid < 0){ perror("fork"); return 1;}


            // Proceso hijo
            if (pid == 0){
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                close(server_fd);
                atender_dron(client_fd, )
            }

            // Proceso padre
            else if (pid > 0){

            }
        }

        // Si llega algo de la pipe:
        if(FD_ISSET(pipe_fd[0], &set)){

        }

        // Mientras queden hijos pendientes los "recojo"
        while(waitpid(-1, NULL, WNOHANG) > 0);
        exit(0);
    }
    

    /* TODO A.3 */

    /* TODO A.4 */

    return 0;
}
