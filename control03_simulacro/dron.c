/*
 * Simulacro Control 03 — Dron CÓNDOR (Parte B)
 *
 * Compilar:  gcc -Wall -Wextra -g -o dron dron.c
 * Ejecutar:  ./dron <id> <ip> [falla]
 *            ./dron 1 127.0.0.1
 *            ./dron 2 127.0.0.1 falla     (el EVT #3 "se pierde" la primera vez)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT       7300
#define BUF_SIZE   256
#define N_EVENTOS  5

/* ---------- DADO ---------- */
const char *eventos[N_EVENTOS] = {
    "despegue",
    "zona A sin novedad",
    "persona detectada en 33.45S 70.66W",
    "solicita equipo de rescate",
    "retorno a base",
};

int reloj = 0;   /* reloj de Lamport del dron */

int max(int a, int b) { return a > b ? a : b; }

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: ./dron <id> <ip> [falla]\n");
        return 1;
    }
    int         id     = atoi(argv[1]);
    const char *ip     = argv[2];
    int         saltar = (argc > 3 && strcmp(argv[3], "falla") == 0);

    /* TODO B.1 */
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) { perror("socket"); return 1;}

    struct sockaddr_in addr = {
        .sin_family= AF_INET,
        .sin_port= htons(PORT)
    };

    if(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0){
        perror("inet_pton");
        return 1;
    }

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("connect");
        return 1;
    }

    char enviar[BUF_SIZE];
    reloj++;
    snprintf(enviar, sizeof(enviar), "JOIN %d %d\n", id, reloj);
    send(client_fd, enviar, strlen(enviar), 0);

    char respuesta[BUF_SIZE];
    // WELCOME O REJECT
    char aceptado[8];
    int id_r;
    int T;
    int seq_sig = 1;
    int n = recv(client_fd, respuesta, sizeof(respuesta) - 1, 0);
    if (n <= 0){
        perror("recv");
        return 1;
    }

    respuesta[n] = '\0';
    sscanf(respuesta, "%s %d %d\n", aceptado, &id_r, &T);
    reloj = max(T, reloj) + 1;
    
    if (strcmp(aceptado, "WELCOME") == 0){
        printf("Conectado a servidor\n");
    }

    else {
        printf("Conexión rechazada, abortando...\n");
        return 1;
    }


    /* TODO B.2 */

    time_t ultimo_hb = time(NULL);
    time_t ultimo_evt = time(NULL);

    while(1){
        fd_set set;
        FD_ZERO(&set);
        FD_SET(client_fd, &set);
        struct timeval tv = {1, 0}; // Timeout
        select(client_fd + 1, &set, NULL, NULL, &tv);
        // Llega algo del servidor
        if(FD_ISSET(client_fd, &set)){
            int s;
            n = recv(client_fd, respuesta, sizeof(respuesta) - 1, 0);
            if (n <= 0){
                perror("recv");
                printf("Servidor caído, desconectando...\n");
                return 1;
            }
            respuesta[n] = '\0';
            sscanf(respuesta, "%s %d %d\n", aceptado, &s, &T);
            reloj = max(T, reloj) + 1;
            // Evento aceptado
            if(strcmp(aceptado, "ACK") == 0){
                seq_sig = s + 1;
            }
            // Evento rechazado
            else if(strcmp(aceptado, "NACK") == 0){
                seq_sig = s;
            }
        }

        // Si pasó más de 1 segundo
        if (time(NULL) - ultimo_hb > 1) {
            reloj++;
            snprintf(enviar, sizeof(enviar), "HB %d %d\n", id, reloj);
            send(client_fd, enviar, strlen(enviar), 0);
            ultimo_hb = time(NULL);
        }

        // Si pasaron más de 2 segundos desde el último EVT
        // Y además aún quedan eventos por mandar
        if (time(NULL) - ultimo_evt > 2 && seq_sig <= N_EVENTOS) {
            reloj++;
            if (saltar == 1 && seq_sig == 3){
                seq_sig++;
                saltar = 0;
                printf("EVT #3 se perdió\n");
                ultimo_evt = time(NULL);
                continue;
            }
            snprintf(enviar, sizeof(enviar), "EVT %d %d %d %s\n", id, seq_sig, reloj, eventos[seq_sig - 1]);
            send(client_fd, enviar, strlen(enviar), 0);
            ultimo_evt = time(NULL);
        }
    }
    /* TODO B.3 */

    return 0;
}
