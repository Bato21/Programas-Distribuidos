/*
 * ============================================================
 *  SIMULACRO CERTAMEN - CENTRAL OVDAS (servidor)  -- solución
 * ============================================================
 *  Es el servidor del Control 01 con otros nombres:
 *    CONNECT/HELLO/REJECT  -> igual que A.1
 *    SISMO + checksum      -> igual que TELEM (A.2)
 *    nivel_alerta()        -> igual que evaluar()
 *    FIN/RESUMEN           -> nuevo: contadores (como el resumen de P1)
 *    fork por cliente      -> igual que A.3
 *
 *  Compilar: gcc -Wall -o central central.c
 * ============================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     7000
#define BUF_SIZE 256

/* ---------- DADO: no se modifica ---------- */
char *checksum(const char *valor, char out[3]) {
    unsigned int s = 0;
    for (int i = 0; valor[i]; i++) s += (unsigned char)valor[i];
    snprintf(out, 3, "%02X", s % 256);
    return out;
}

typedef struct {
    char  volcan[16];
    float amarilla;
    float naranja;
    float roja;
} Umbral;

Umbral umbrales[] = {
    {"VILLARRICA", 2.0, 3.5, 5.0},
    {"LLAIMA",     2.5, 4.0, 5.5},
    {"CALBUCO",    2.0, 3.0, 4.5},
    {"LASCAR",     3.0, 4.5, 6.0},
};
/* ------------------------------------------ */

#define N_VOLCANES (int)(sizeof(umbrales) / sizeof(umbrales[0]))

/* Se revisa de la más grave a la menos grave (igual que evaluar() del control) */
const char *nivel_alerta(const Umbral *u, float m) {
    if (m >= u->roja)     return "ROJA";
    if (m >= u->naranja)  return "NARANJA";
    if (m >= u->amarilla) return "AMARILLA";
    return "VERDE";
}

void handle_estacion(int fd) {
    char buf[BUF_SIZE];
    char resp[BUF_SIZE];

    /* ===================== TODO A.1 ===================== */
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    char volcan[16];
    if (sscanf(buf, "CONNECT %15s", volcan) != 1) {
        send(fd, "REJECT ?\n", 9, 0);
        return;
    }

    Umbral *u = NULL;
    for (int i = 0; i < N_VOLCANES; i++) {
        if (strcmp(volcan, umbrales[i].volcan) == 0) {
            u = &umbrales[i];
            break;
        }
    }

    if (u == NULL) {
        snprintf(resp, sizeof(resp), "REJECT %s\n", volcan);
        send(fd, resp, strlen(resp), 0);
        printf("[CENTRAL] Rechazada estación desconocida: %s\n", volcan);
        return;
    }

    snprintf(resp, sizeof(resp), "HELLO %s\n", volcan);
    send(fd, resp, strlen(resp), 0);
    printf("[CENTRAL] %s conectada\n", volcan);

    /* ===================== TODO A.2 ===================== */
    int recibidas = 0, alertas = 0;     /* propios de ESTE hijo (fork copia memoria) */

    while (1) {
        n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("[CENTRAL] %s se desconectó\n", volcan);
            break;
        }
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';

        /* ¿Qué comando es? Primera palabra (como el HELLO de la sonda) */
        char cmd[16];
        if (sscanf(buf, "%15s", cmd) != 1) continue;

        if (strcmp(cmd, "FIN") == 0) {
            snprintf(resp, sizeof(resp), "RESUMEN %s %d %d\n", volcan, recibidas, alertas);
            send(fd, resp, strlen(resp), 0);
            printf("[CENTRAL] %s FIN: recibidas=%d alertas=%d\n", volcan, recibidas, alertas);
            break;
        }

        char s_volcan[16], mag_str[32], cs_recibido[3], cs_calculado[3];
        int seq = 0;

        /* La magnitud se lee como TEXTO: el checksum es sobre los caracteres exactos */
        int campos = sscanf(buf, "SISMO %15s %d %31s %2s", s_volcan, &seq, mag_str, cs_recibido);

        if (campos != 4) {
            snprintf(resp, sizeof(resp), "NACK %s %d\n", volcan, seq);
            send(fd, resp, strlen(resp), 0);
            continue;
        }

        checksum(mag_str, cs_calculado);

        if (strcmp(s_volcan, volcan) != 0 || strcmp(cs_recibido, cs_calculado) != 0) {
            snprintf(resp, sizeof(resp), "NACK %s %d\n", volcan, seq);
            send(fd, resp, strlen(resp), 0);
            printf("[CENTRAL] %s seq=%d NACK\n", volcan, seq);
            continue;
        }

        float m = atof(mag_str);
        const char *nivel = nivel_alerta(u, m);

        recibidas++;
        if (strcmp(nivel, "VERDE") != 0) alertas++;

        /* UNA sola respuesta por mensaje: así cliente y servidor no se desfasan */
        snprintf(resp, sizeof(resp), "ACK %s %d %s\n", volcan, seq, nivel);
        send(fd, resp, strlen(resp), 0);
        printf("[CENTRAL] %s seq=%d M=%.2f -> %s\n", volcan, seq, m, nivel);
    }
}

int main(void) {
    /* ===================== TODO A.3 ===================== */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("[CENTRAL] Escuchando en puerto %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {                 /* HIJO: atiende a UNA estación */
            close(server_fd);           /* el hijo no acepta conexiones */
            handle_estacion(client_fd);
            close(client_fd);
            exit(0);
        }

        /* PADRE: no habla con la estación, solo vuelve a accept */
        close(client_fd);
        while (waitpid(-1, NULL, WNOHANG) > 0);   /* recoger zombis */
    }

    close(server_fd);
    return 0;
}
