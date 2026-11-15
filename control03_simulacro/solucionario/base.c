/*
 * SOLUCIONARIO — Simulacro Control 03 — Base CÓNDOR
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
#define TIMEOUT_HB  3                      /* segundos sin HB => CAIDO */

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
#define LIBRE   0     /* nunca se conectó o se fue con BYE */
#define VIVO    1
#define CAIDO   2

typedef struct {
    int    fd;          /* socket del dron (para que el padre responda) */
    int    estado;      /* LIBRE, VIVO o CAIDO                          */
    time_t ultimo_hb;   /* última vez que se supo de él                 */
    int    ultimo_seq;  /* último EVT aplicado al log (0 = ninguno)     */
} Dron;

Dron drones[N_DRONES + 1];   /* índice 1..N_DRONES (el 0 no se usa) */
int  reloj = 0;              /* reloj de Lamport de la base: SOLO el padre lo toca */

/* ---------- helpers del solucionario ---------- */
static int max(int a, int b) { return a > b ? a : b; }

static int contar_vivos(void) {
    int v = 0;
    for (int i = 1; i <= N_DRONES; i++)
        if (drones[i].estado == VIVO) v++;
    return v;
}

/* ================= TODO A.2: el hijo "cartero" ================= */
void atender_dron(int fd, int id, int pipe_w) {
    char buf[BUF_SIZE];
    Aviso a;

    while (1) {
        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;                         /* el dron cerró o murió */
        buf[n] = '\0';

        /* un recv puede traer más de una línea (HB + EVT juntos): procesar todas */
        char *save = NULL;
        for (char *linea = strtok_r(buf, "\n", &save); linea; linea = strtok_r(NULL, "\n", &save)) {
            memset(&a, 0, sizeof(a));
            int idm;
            if (sscanf(linea, "HB %d %d", &idm, &a.T) == 2) {
                a.tipo = T_HB;
            } else if (sscanf(linea, "EVT %d %d %d %127[^\n]", &idm, &a.seq, &a.T, a.texto) == 4) {
                a.tipo = T_EVT;
            } else {
                continue;                          /* línea mal formada: se ignora */
            }
            if (idm != id) continue;               /* no puede hablar por otro dron */
            a.id = id;
            write(pipe_w, &a, sizeof(a));
        }
    }

    memset(&a, 0, sizeof(a));
    a.id = id;
    a.tipo = T_BYE;
    write(pipe_w, &a, sizeof(a));
}

int main(void) {
    /* ================= TODO A.1: inicialización ================= */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, N_DRONES) < 0) { perror("listen"); return 1; }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) { perror("pipe"); return 1; }

    memset(drones, 0, sizeof(drones));
    int hay_quorum = 0;
    printf("[BASE] Escuchando en puerto %d (quórum = %d de %d)\n", PORT, QUORUM, N_DRONES);

    while (1) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(server_fd, &set);
        FD_SET(pipe_fd[0], &set);
        int maxfd = max(server_fd, pipe_fd[0]);
        struct timeval tv = {1, 0};                /* despertar cada 1 s para revisar HB */

        if (select(maxfd + 1, &set, NULL, NULL, &tv) < 0) { perror("select"); break; }

        /* ---------- A.1: llega un dron nuevo ---------- */
        if (FD_ISSET(server_fd, &set)) {
            int cfd = accept(server_fd, NULL, NULL);
            if (cfd < 0) { perror("accept"); continue; }

            char buf[BUF_SIZE], resp[BUF_SIZE];
            int id = 0, T = 0;
            int n = recv(cfd, buf, sizeof(buf) - 1, 0);
            if (n <= 0) { close(cfd); continue; }
            buf[n] = '\0';

            int ok = (sscanf(buf, "JOIN %d %d", &id, &T) == 2);
            reloj = max(reloj, T) + 1;             /* recepción */
            reloj++;                               /* envío de la respuesta */

            if (!ok || id < 1 || id > N_DRONES || drones[id].estado != LIBRE) {
                snprintf(resp, sizeof(resp), "REJECT %d %d\n", id, reloj);
                send(cfd, resp, strlen(resp), 0);
                printf("[BASE] [L=%d] JOIN rechazado (id=%d)\n", reloj, id);
                close(cfd);
            } else {
                snprintf(resp, sizeof(resp), "WELCOME %d %d\n", id, reloj);
                send(cfd, resp, strlen(resp), 0);
                drones[id].fd = cfd;
                drones[id].estado = VIVO;
                drones[id].ultimo_hb = time(NULL);
                /* ultimo_seq se conserva si el dron se reconecta: el log no se pierde */
                printf("[BASE] [L=%d] DRON-%d se unió (vivos=%d)\n", reloj, id, contar_vivos());

                fflush(stdout);
                pid_t pid = fork();
                if (pid < 0) { perror("fork"); close(cfd); drones[id].estado = LIBRE; continue; }
                if (pid == 0) {
                    close(server_fd);
                    close(pipe_fd[0]);
                    for (int i = 1; i <= N_DRONES; i++)       /* sockets ajenos */
                        if (i != id && drones[i].estado != LIBRE) close(drones[i].fd);
                    atender_dron(cfd, id, pipe_fd[1]);
                    close(cfd);
                    close(pipe_fd[1]);
                    exit(0);
                }
                /* el padre conserva cfd para poder responder ACK/NACK */
            }
        }

        /* ---------- A.3: llega un aviso de algún hijo ---------- */
        if (FD_ISSET(pipe_fd[0], &set)) {
            Aviso a;
            if (read(pipe_fd[0], &a, sizeof(a)) == sizeof(a)) {
                Dron *d = &drones[a.id];

                if (a.tipo == T_BYE) {
                    printf("[BASE] [L=%d] DRON-%d se desconectó\n", reloj, a.id);
                    close(d->fd);
                    d->estado = LIBRE;
                } else {
                    reloj = max(reloj, a.T) + 1;  /* recepción (HB y EVT son mensajes) */
                    d->ultimo_hb = time(NULL);    /* cualquier mensaje prueba que está vivo */
                    if (d->estado == CAIDO) {
                        d->estado = VIVO;
                        printf("[BASE] [L=%d] DRON-%d VOLVIÓ\n", reloj, a.id);
                    }

                    if (a.tipo == T_EVT) {
                        char resp[BUF_SIZE];
                        reloj++;                  /* envío de ACK/NACK */
                        if (hay_quorum && a.seq == d->ultimo_seq + 1) {
                            d->ultimo_seq = a.seq;
                            printf("[BASE] [L=%d] LOG DRON-%d #%d: %s\n", reloj, a.id, a.seq, a.texto);
                            snprintf(resp, sizeof(resp), "ACK %d %d\n", a.seq, reloj);
                        } else if (a.seq <= d->ultimo_seq) {
                            /* duplicado (ya estaba aplicado): se confirma sin volver a aplicarlo */
                            snprintf(resp, sizeof(resp), "ACK %d %d\n", a.seq, reloj);
                        } else {
                            int esperado = d->ultimo_seq + 1;
                            if (hay_quorum)
                                printf("[BASE] [L=%d] NACK DRON-%d: llegó #%d, espero #%d\n",
                                       reloj, a.id, a.seq, esperado);
                            else
                                printf("[BASE] [L=%d] NACK DRON-%d: EVT #%d en espera (SIN QUÓRUM)\n",
                                       reloj, a.id, a.seq);
                            snprintf(resp, sizeof(resp), "NACK %d %d\n", esperado, reloj);
                        }
                        send(d->fd, resp, strlen(resp), 0);
                    }
                }
            }
        }

        /* ================= TODO A.4: heartbeats y quórum ================= */
        time_t ahora = time(NULL);
        for (int i = 1; i <= N_DRONES; i++) {
            if (drones[i].estado == VIVO && ahora - drones[i].ultimo_hb > TIMEOUT_HB) {
                drones[i].estado = CAIDO;
                printf("[BASE] DRON-%d CAÍDO (sin heartbeat hace %lds)\n",
                       i, (long)(ahora - drones[i].ultimo_hb));
            }
        }
        int vivos = contar_vivos();
        if (!hay_quorum && vivos >= QUORUM) {
            hay_quorum = 1;
            printf("[BASE] QUÓRUM OK (vivos=%d/%d): se aceptan eventos\n", vivos, N_DRONES);
        } else if (hay_quorum && vivos < QUORUM) {
            hay_quorum = 0;
            printf("[BASE] SIN QUÓRUM (vivos=%d/%d): eventos en espera\n", vivos, N_DRONES);
        }

        while (waitpid(-1, NULL, WNOHANG) > 0) ;   /* recoger hijos terminados */
        fflush(stdout);
    }

    close(server_fd);
    return 0;
}
