/*
 * SOLUCIONARIO — Simulacro Control 03 — Dron CÓNDOR
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

int reloj = 0;   /* reloj de Lamport del dron (un solo proceso => un solo reloj) */

static int max(int a, int b) { return a > b ? a : b; }

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: ./dron <id> <ip> [falla]\n");
        return 1;
    }
    int         id     = atoi(argv[1]);
    const char *ip     = argv[2];
    int         saltar = (argc > 3 && strcmp(argv[3], "falla") == 0);

    /* ================= TODO B.1: conexión y JOIN ================= */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    if (inet_pton(AF_INET, ip, &srv.sin_addr) <= 0) { fprintf(stderr, "IP inválida\n"); return 1; }
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) { perror("connect"); return 1; }

    char buf[BUF_SIZE], msg[BUF_SIZE];
    reloj++;                                              /* envío */
    snprintf(msg, sizeof(msg), "JOIN %d %d\n", id, reloj);
    send(sock, msg, strlen(msg), 0);

    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { printf("La base cerró la conexión\n"); close(sock); return 1; }
    buf[n] = '\0';

    char tipo[16];
    int idr, T;
    if (sscanf(buf, "%15s %d %d", tipo, &idr, &T) != 3) { printf("Respuesta inválida\n"); close(sock); return 1; }
    reloj = max(reloj, T) + 1;                            /* recepción */
    if (strcmp(tipo, "WELCOME") != 0) {
        printf("[DRON-%d] [L=%d] Rechazado por la base\n", id, reloj);
        close(sock);
        return 1;
    }
    printf("[DRON-%d] [L=%d] Conectado a la base\n", id, reloj);

    /* ================= TODO B.2 / B.3: loop con select ================= */
    int    seq_sig    = 1;          /* próximo EVT que hay que lograr que la base confirme */
    time_t ultimo_hb  = 0;          /* 0 => manda HB y EVT apenas parte */
    time_t ultimo_evt = 0;
    int    terminado  = 0;

    while (1) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);
        struct timeval tv = {1, 0};

        if (select(sock + 1, &set, NULL, NULL, &tv) < 0) { perror("select"); break; }

        /* ---------- B.3: respuestas de la base ---------- */
        if (FD_ISSET(sock, &set)) {
            n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) { printf("[DRON-%d] La base cerró la conexión\n", id); break; }
            buf[n] = '\0';

            char *save = NULL;
            for (char *l = strtok_r(buf, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
                int s;
                if (sscanf(l, "%15s %d %d", tipo, &s, &T) != 3) continue;
                reloj = max(reloj, T) + 1;                /* recepción */
                if (strcmp(tipo, "ACK") == 0) {
                    printf("[DRON-%d] [L=%d] ACK #%d\n", id, reloj, s);
                    if (s + 1 > seq_sig) seq_sig = s + 1;
                } else if (strcmp(tipo, "NACK") == 0) {
                    printf("[DRON-%d] [L=%d] NACK: la base espera #%d\n", id, reloj, s);
                    seq_sig = s;                          /* retroceder y reenviar desde ahí */
                }
            }
            if (seq_sig > N_EVENTOS && !terminado) {
                terminado = 1;
                printf("[DRON-%d] Todos los eventos confirmados. Sigo en vuelo (solo HB).\n", id);
            }
        }

        /* ---------- B.2: tareas periódicas ---------- */
        time_t ahora = time(NULL);

        if (ahora - ultimo_hb >= 1) {
            reloj++;                                      /* envío */
            snprintf(msg, sizeof(msg), "HB %d %d\n", id, reloj);
            send(sock, msg, strlen(msg), 0);
            ultimo_hb = ahora;
        }

        if (seq_sig <= N_EVENTOS && ahora - ultimo_evt >= 2) {
            ultimo_evt = ahora;
            if (saltar && seq_sig == 3) {
                saltar = 0;
                printf("[DRON-%d] (simulación) el EVT #3 se perdió en el aire\n", id);
                seq_sig = 4;                              /* creemos que lo mandamos */
            } else {
                reloj++;                                  /* envío */
                snprintf(msg, sizeof(msg), "EVT %d %d %d %s\n", id, seq_sig, reloj, eventos[seq_sig - 1]);
                send(sock, msg, strlen(msg), 0);
                printf("[DRON-%d] [L=%d] EVT #%d enviado\n", id, reloj, seq_sig);
            }
        }
        fflush(stdout);
    }

    close(sock);
    return 0;
}
