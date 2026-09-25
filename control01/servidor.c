/*
 * ============================================================
 *  CONTROL 01 - BASE SELENE (servidor)   -- solución comentada
 * ============================================================
 *
 *  Compilar:  gcc -Wall -Wextra -g -o servidor servidor.c
 *  Ejecutar:  ./servidor
 *
 *  Protocolo (cada mensaje termina en \n):
 *    Sonda -> Base : CONNECT <id>
 *    Base -> Sonda : HELLO <id>   |  REJECT <id>
 *    Sonda -> Base : TELEM <id> <seq> <valor> <cs>
 *    Base -> Sonda : ACK <id> <seq>  |  NACK <id> <seq>
 *                    ALERT <id> <nivel> <detalle>
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

#define PORT     7070
#define BUF_SIZE 256

/* ---------- DADO: no se modifica ---------- */
char *checksum(const char *valor, char out[3]) {
    unsigned int s = 0;
    for (int i = 0; valor[i]; i++) s += (unsigned char)valor[i];
    snprintf(out, 3, "%02X", s % 256);
    return out;
}

typedef struct {
    char  id[8];
    float alerta;
    float critico;
    int   mayor;   /* 1: alerta si valor > umbral | 0: si valor < umbral */
} Umbral;

Umbral umbrales[] = {
    {"ARES-1", -40, -30, 1},
    {"ARES-2", 3.5, 6.0, 1},
    {"ARES-3", 750, 900, 1},
    {"ARES-4",  30,  15, 0},
};
/* ------------------------------------------ */

#define N_SONDAS (int)(sizeof(umbrales) / sizeof(umbrales[0]))   /* = 4 */

/*
 * evaluar(): parte del TODO A.2.
 * Devuelve "NORMAL", "ALERTA" o "CRITICO" según el umbral de la sonda.
 * Se revisa CRÍTICO primero: si un valor es crítico también supera la
 * alerta, y queremos reportar el nivel más grave.
 */
const char *evaluar(const Umbral *u, float valor) {
    if (u->mayor) {                       /* ARES-1, 2, 3: malo si SUBE */
        if (valor > u->critico) return "CRITICO";
        if (valor > u->alerta)  return "ALERTA";
    } else {                              /* ARES-4: malo si BAJA */
        if (valor < u->critico) return "CRITICO";
        if (valor < u->alerta)  return "ALERTA";
    }
    return "NORMAL";
}

void handle_sonda(int fd) {
    char buf[BUF_SIZE];

    /* ===================== TODO A.1 =====================
     * Recibir "CONNECT <id>", validar el id, responder HELLO o REJECT.
     */
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;                   /* la sonda se fue sin hablar */
    buf[n] = '\0';                        /* recv NO pone el '\0' */
    buf[strcspn(buf, "\r\n")] = '\0';     /* quitar el '\n' final */

    char id[8];
    if (sscanf(buf, "CONNECT %7s", id) != 1) {
        send(fd, "REJECT ?\n", 9, 0);     /* mensaje mal formado */
        return;
    }

    /* buscar el id en la tabla de umbrales */
    Umbral *u = NULL;
    for (int i = 0; i < N_SONDAS; i++) {
        if (strcmp(id, umbrales[i].id) == 0) {
            u = &umbrales[i];
            break;
        }
    }

    char resp[BUF_SIZE];
    if (u == NULL) {
        snprintf(resp, sizeof(resp), "REJECT %s\n", id);
        send(fd, resp, strlen(resp), 0);
        printf("[BASE] Rechazada sonda desconocida: %s\n", id);
        return;
    }

    snprintf(resp, sizeof(resp), "HELLO %s\n", id);
    send(fd, resp, strlen(resp), 0);
    printf("[BASE] %s conectada\n", id);

    /* ===================== TODO A.2 =====================
     * Loop: recibir TELEM, validar checksum, ACK/NACK, evaluar umbral, ALERT.
     */
    while (1) {
        n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;                /* 0 = la sonda cerró, <0 = error */
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';

        char t_id[8], valor_str[32], cs_recibido[3], cs_calculado[3];
        int  seq;

        /*
         * El valor se lee como TEXTO (%31s), no como %f: el checksum se
         * calcula sobre los caracteres exactos que mandó la sonda.
         * sscanf devuelve cuántos campos logró leer -> deben ser 4.
         */
        int campos = sscanf(buf, "TELEM %7s %d %31s %2s",
                            t_id, &seq, valor_str, cs_recibido);

        if (campos != 4) {
            /* sin seq válido no podemos armar un NACK decente */
            snprintf(resp, sizeof(resp), "NACK %s 0\n", id);
            send(fd, resp, strlen(resp), 0);
            continue;
        }

        checksum(valor_str, cs_calculado);

        /* NACK si el checksum no calza o si el id no es el de esta conexión */
        if (strcmp(cs_recibido, cs_calculado) != 0 || strcmp(t_id, id) != 0) {
            snprintf(resp, sizeof(resp), "NACK %s %d\n", id, seq);
            send(fd, resp, strlen(resp), 0);
            printf("[BASE] %s seq=%d NACK (cs recibido=%s, esperado=%s)\n",
                   id, seq, cs_recibido, cs_calculado);
            continue;
        }

        float valor = atof(valor_str);
        const char *nivel = evaluar(u, valor);

        /*
         * Armamos ACK (y ALERT si corresponde) en UN solo buffer y hacemos
         * UN solo send. Así la sonda, que hace un recv por lectura, recibe
         * todo junto y no queda un ALERT "colgado" para la siguiente vuelta.
         */
        int len = snprintf(resp, sizeof(resp), "ACK %s %d\n", id, seq);

        if (strcmp(nivel, "NORMAL") != 0) {
            float limite = (strcmp(nivel, "CRITICO") == 0) ? u->critico : u->alerta;
            snprintf(resp + len, sizeof(resp) - len,
                     "ALERT %s %s valor=%.2f umbral=%.2f\n",
                     id, nivel, valor, limite);
        }

        send(fd, resp, strlen(resp), 0);
        printf("[BASE] %s seq=%d valor=%.2f -> %s\n", id, seq, valor, nivel);
    }

    printf("[BASE] %s desconectada\n", id);
}

int main(void) {

    /* ===================== TODO A.3 =====================
     * socket -> bind -> listen -> (accept + fork) en loop
     */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, 10) < 0) { perror("listen"); return 1; }

    printf("[BASE] SELENE escuchando en puerto %d\n", PORT);
    fflush(stdout);                       /* vaciar antes de fork() */

    while (1) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);

        int client_fd = accept(server_fd, (struct sockaddr *)&cli, &cli_len);
        if (client_fd < 0) { perror("accept"); continue; }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); close(client_fd); continue; }

        if (pid == 0) {
            /* ===== HIJO: atiende a UNA sonda ===== */
            close(server_fd);             /* el hijo no acepta conexiones */
            handle_sonda(client_fd);
            close(client_fd);
            exit(0);
        }

        /* ===== PADRE: vuelve a accept() de inmediato ===== */
        close(client_fd);                 /* la copia del padre no se usa */

        /* recoger hijos que ya terminaron (evita procesos zombie) */
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    close(server_fd);
    return 0;
}
