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
int hay_quorum = 0;          /* condicion de quorum */

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

            /* A.2 (escrito por Claude) */
            memset(&a, 0, sizeof(a));
            int idm;
            if (sscanf(linea, "HB %d %d", &idm, &a.T) == 2) {
                a.tipo = T_HB;
            } else if (sscanf(linea, "EVT %d %d %d %127[^\n]", &idm, &a.seq, &a.T, a.texto) == 4) {
                a.tipo = T_EVT;
            } else {
                continue;                       /* línea mal formada: se ignora */
            }
            if (idm != id) continue;            /* no puede hablar por otro dron */
            a.id = id;
            write(pipe_w, &a, sizeof(a));
        }
    }

    /* A.2 (fin): el dron se fue => avisar al padre */
    memset(&a, 0, sizeof(a));
    a.id = id;
    a.tipo = T_BYE;
    write(pipe_w, &a, sizeof(a));
}

int main(void) {

    /* TODO A.1 */

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1){ 
        perror("pipe");
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // **
    if (server_fd < 0){ 
        perror("socket"); 
        return 1; // **
    }

    // Reusar puerto
    int opt = 1; // **
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // **

    // **
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind"); 
        return 1;
    }

    if (listen(server_fd, N_DRONES) < 0){
        perror("listen");
        return 1;
    }

    printf("[SERVER] Escuchando en puerto: %d...\n", PORT);

    // Aceptamos conexiones
    while(1){
        fd_set set;
        FD_ZERO(&set);
        FD_SET(server_fd, &set);
        FD_SET(pipe_fd[0], &set);
        int max_fd = max(server_fd, pipe_fd[0]);
        // Timeout de 1 segundo
        struct timeval tv = {1, 0};
        select(max_fd + 1, &set, NULL, NULL, &tv);
        // Si llega algo del servidor
        if (FD_ISSET(server_fd, &set)){
            int client_fd = accept(server_fd, NULL, NULL);
            char recibido[BUF_SIZE];
            char enviado[BUF_SIZE];
            int id;
            int T;
            
            int n = recv(client_fd, recibido, sizeof(recibido) - 1, 0);
            if (n <= 0) { 
                perror("recv");
                close(client_fd);
                continue;
            }

            recibido[n] = '\0';
            sscanf(recibido, "JOIN %d %d", &id, &T);
            reloj = max(reloj, T) + 1;
            reloj++;
            // Id inválido o ocupado
            if (id < 1 || id > 5 || drones[id].estado != LIBRE){
                snprintf(enviado, sizeof(enviado), "REJECT %d %d\n", id, reloj);
                send(client_fd, enviado, strlen(enviado), 0);
                close(client_fd);
                continue;
            }

            // Conexion exitosa
            drones[id].estado = VIVO;
            drones[id].fd = client_fd;
            drones[id].ultimo_hb = time(NULL);
            snprintf(enviado, sizeof(enviado), "WELCOME %d %d\n", id, reloj);
            send(client_fd, enviado, strlen(enviado), 0);
            

            
            pid_t pid = fork();
            if(pid < 0) {
                perror("fork");
                return 1;
            }

            // Proceso hijo
            else if (pid == 0){
                close(server_fd);
                close(pipe_fd[0]);
                atender_dron(client_fd, id, pipe_fd[1]);
                close(client_fd);
                close(pipe_fd[1]);
                exit(0);
            }

            // Proceso padre
            else if (pid > 0){

            }
            
        }

        // Si llega algo del pipe
        if (FD_ISSET(pipe_fd[0], &set)){
            // Recibimos un Aviso completo
            Aviso a;
            char buf[BUF_SIZE];

            read(pipe_fd[0], &a, sizeof(Aviso));

            if(a.tipo == T_BYE){
                // Cerramos fd del dron
                close(drones[a.id].fd);
                drones[a.id].estado = LIBRE;
            }

            if(a.tipo == T_HB || a.tipo == T_EVT){
                reloj = max(reloj, a.T) + 1;
                drones[a.id].ultimo_hb = time(NULL);
                if(drones[a.id].estado == CAIDO){
                    drones[a.id].estado = VIVO;
                    printf("DRON-%d VOLVIÓ\n", a.id);
                }

                if(a.tipo == T_EVT){
                    // Enviamos
                    reloj++;
                    if (hay_quorum == 1 && a.seq == drones[a.id].ultimo_seq + 1){
                        printf("[L=%d] LOG DRON-%d #%d:%s\n", reloj, a.id, a.seq, a.texto);
                        drones[a.id].ultimo_seq = a.seq;
                        snprintf(buf, sizeof(buf), "ACK %d %d\n", a.seq, reloj);
                    }
                    else if (a.seq <= drones[a.id].ultimo_seq){
                        snprintf(buf, sizeof(buf), "ACK %d %d\n", a.seq, reloj);
                    }
                    else {
                        snprintf(buf, sizeof(buf), "NACK %d %d\n", drones[a.id].ultimo_seq + 1, reloj);
                    }
                    send(drones[a.id].fd, buf, strlen(buf), 0);
                }
            }

            
        }

        // Recorremos para ver si hay drones muertos
        for (int i = 1; i < N_DRONES + 1; i++){
            if(drones[i].estado == VIVO && time(NULL) - drones[i].ultimo_hb > TIMEOUT_HB){
                drones[i].estado = CAIDO;
                printf("DRON %d CAIDO\n", i);
            }
        }

        if (hay_quorum == 1 && contar_vivos() < QUORUM){
            hay_quorum = 0;
            printf("SIN QUÓRUM (vivos=%d/5)\n", contar_vivos());
        }

        if (hay_quorum == 0 && contar_vivos() >= QUORUM){
            hay_quorum = 1;
            printf("QUÓRUM OK (vivos=%d/5)\n", contar_vivos());
        }

        while(waitpid(-1, NULL, WNOHANG) > 0);
    }
    

    /* TODO A.3 */

    

    /* TODO A.4 */

    return 0;
}
