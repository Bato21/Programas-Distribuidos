/*
 * ============================================================
 *  SIMULACRO CERTAMEN - ESTACIÓN SISMOLÓGICA  -- solución
 * ============================================================
 *
 *   SISMOGRAFO (hijo 1) --pipe1--> FILTRO (hijo 2) --pipe2--> TRANSMISOR (padre) --TCP--> CENTRAL
 *
 *  - B.1 + B.2 + B.3 son la P2 de la guía (Rocinante) casi igual.
 *  - B.4 es la sonda del Control 01, pero leyendo los datos desde un pipe
 *    en vez de inventarlos (igual que el Analizador del ej. 3 del certamen).
 *
 *  Compilar: gcc -Wall -o estacion estacion.c
 *  Ejecutar: ./estacion VILLARRICA      (con la central ya corriendo)
 * ============================================================
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

    /* ===================== TODO B.1 =====================
     * Los 2 pipes ANTES de cualquier fork (así los 3 procesos los heredan).
     */
    int pipe1[2];   /* sismógrafo -> filtro     */
    int pipe2[2];   /* filtro     -> transmisor */

    if (pipe(pipe1) == -1) {
        perror("pipe");
        return 1;
    }
    if (pipe(pipe2) == -1) {
        perror("pipe");
        return 1;
    }

    /* ===================== TODO B.2: SISMOGRAFO (hijo 1) ===================== */
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        return 1;
    }

    if (pid1 == 0) {
        /* Solo escribe en pipe1: cierra todo lo demás */
        close(pipe1[0]);
        close(pipe2[0]);
        close(pipe2[1]);

        srand(getpid());   /* semilla distinta por proceso/estación */

        char msg[BUF_SIZE];
        for (int seq = 1; seq <= N_LECTURAS; seq++) {
            float m = (float)rand() / RAND_MAX * 8.0 - 1.0;   /* -1.0 a 7.0 */

            snprintf(msg, sizeof(msg), "LECT %d %.2f\n", seq, m);
            write(pipe1[1], msg, strlen(msg));
            printf("[SISMOGRAFO] LECT %d %.2f\n", seq, m);

            sleep(1);   /* además evita que 2 mensajes lleguen pegados en un read */
        }

        close(pipe1[1]);   /* -> el read del filtro devolverá 0 */
        exit(0);
    }

    /* ===================== TODO B.3: FILTRO (hijo 2) =====================
     * Solo el PADRE llega aquí, así que solo el padre hace este 2do fork.
     */
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        return 1;
    }

    if (pid2 == 0) {
        /* Lee de pipe1 y escribe en pipe2 */
        close(pipe1[1]);   /* CLAVE: si no, el read de pipe1 nunca devuelve 0 */
        close(pipe2[0]);

        char buf[BUF_SIZE];
        char msg[BUF_SIZE];
        ssize_t n;

        while ((n = read(pipe1[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';

            int seq;
            float m;
            if (sscanf(buf, "LECT %d %f", &seq, &m) != 2) continue;

            if (m < 0.0) {
                printf("[FILTRO] Descartada seq=%d (falla de sensor: %.2f)\n", seq, m);
            } else if (m < 1.0) {
                printf("[FILTRO] Descartada seq=%d (ruido: %.2f)\n", seq, m);
            } else {
                snprintf(msg, sizeof(msg), "VALIDA %d %.2f\n", seq, m);
                write(pipe2[1], msg, strlen(msg));
            }
        }

        close(pipe1[0]);
        close(pipe2[1]);   /* -> el read del transmisor devolverá 0 */
        exit(0);
    }

    /* ===================== TODO B.4: TRANSMISOR (padre) ===================== */
    /* Solo lee de pipe2 */
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[1]);

    /* --- Cliente TCP (igual que B.1 de la sonda) --- */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        close(pipe2[0]);
        wait(NULL);
        wait(NULL);
        return 1;
    }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
    };
    inet_pton(AF_INET, IP, &srv.sin_addr);

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(sock);
        close(pipe2[0]);   /* filtro recibe SIGPIPE al escribir -> muere; luego el sismógrafo igual */
        wait(NULL);
        wait(NULL);
        return 1;
    }

    /* --- CONNECT / HELLO (igual que B.2 de la sonda) --- */
    char buf[BUF_SIZE];
    char msg[BUF_SIZE];
    ssize_t n;

    snprintf(msg, sizeof(msg), "CONNECT %s\n", volcan);
    send(sock, msg, strlen(msg), 0);

    n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        printf("[TRANSMISOR] La Central cerró la conexión\n");
        close(sock);
        close(pipe2[0]);
        wait(NULL);
        wait(NULL);
        return 1;
    }
    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    char tipo[16];
    sscanf(buf, "%15s", tipo);
    if (strcmp(tipo, "HELLO") != 0) {
        printf("[TRANSMISOR] La Central rechazó la conexión: %s\n", buf);
        close(sock);
        close(pipe2[0]);
        wait(NULL);
        wait(NULL);
        return 1;
    }
    printf("[TRANSMISOR] Conectado a la Central como %s\n", volcan);

    /* --- Loop: pipe2 -> socket -> respuesta (como P1/P2 + B.3 de la sonda) --- */
    char resp[BUF_SIZE];
    while ((n = read(pipe2[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';

        int seq;
        float m;
        if (sscanf(buf, "VALIDA %d %f", &seq, &m) != 2) continue;

        char mag_txt[16], cs[3];
        snprintf(mag_txt, sizeof(mag_txt), "%.2f", m);   /* texto exacto que va al checksum */
        checksum(mag_txt, cs);

        snprintf(msg, sizeof(msg), "SISMO %s %d %s %s\n", volcan, seq, mag_txt, cs);
        if (send(sock, msg, strlen(msg), 0) < 0) {
            perror("send");
            break;
        }

        n = recv(sock, resp, sizeof(resp) - 1, 0);
        if (n <= 0) {
            printf("[TRANSMISOR] La Central cerró la conexión\n");
            break;
        }
        resp[n] = '\0';
        printf("[TRANSMISOR] %s", resp);   /* resp ya trae su '\n' */
    }

    /* --- FIN / RESUMEN --- */
    snprintf(msg, sizeof(msg), "FIN %s\n", volcan);
    send(sock, msg, strlen(msg), 0);

    n = recv(sock, resp, sizeof(resp) - 1, 0);
    if (n > 0) {
        resp[n] = '\0';
        printf("[TRANSMISOR] %s", resp);
    }

    close(sock);
    close(pipe2[0]);
    wait(NULL);   /* sismógrafo */
    wait(NULL);   /* filtro     */

    printf("[TRANSMISOR] Estación finalizada\n");
    return 0;
}
