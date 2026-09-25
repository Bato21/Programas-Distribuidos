/*
 * ============================================================
 *  CONTROL 01 - SONDA ARES (cliente)   -- solución comentada
 * ============================================================
 *
 *  Compilar:  gcc -Wall -Wextra -g -o sonda sonda.c
 *  Ejecutar:  ./sonda ARES-1 127.0.0.1 -45
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
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

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: ./sonda <ARES-N> <IP> <valor_inicial>\n");
        return 1;
    }
    const char *id    = argv[1];
    const char *ip    = argv[2];
    float       valor = atof(argv[3]);

    /* ===================== TODO B.1 =====================
     * socket -> llenar dirección -> inet_pton -> connect
     */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT)
    };
    if (inet_pton(AF_INET, ip, &srv.sin_addr) <= 0) {
        fprintf(stderr, "IP inválida: %s\n", ip);
        close(sock);
        return 1;
    }
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    /* ===================== TODO B.2 =====================
     * Enviar CONNECT <id>, esperar HELLO o REJECT
     */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "CONNECT %s\n", id);
    send(sock, buf, strlen(buf), 0);

    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        printf("El servidor cerró la conexión\n");
        close(sock);
        return 1;
    }
    buf[n] = '\0';

    char tipo[16];
    sscanf(buf, "%15s", tipo);            /* primera palabra: HELLO o REJECT */

    if (strcmp(tipo, "HELLO") != 0) {
        printf("Conexión rechazada: %s", buf);
        close(sock);
        return 1;
    }
    printf("Conectado: %s", buf);

    /* ===================== TODO B.3 =====================
     * 8 lecturas, seq 1..8, valor cambia, recv + print, sleep(1)
     */
    srand(getpid());                      /* semilla distinta por sonda */

    for (int seq = 1; seq <= 8; seq++) {
        char valor_str[32], cs[3];

        /* el valor pasa a texto UNA vez; ese mismo texto va al checksum
           y al mensaje, así la base calcula exactamente lo mismo */
        snprintf(valor_str, sizeof(valor_str), "%.2f", valor);
        checksum(valor_str, cs);

        snprintf(buf, sizeof(buf), "TELEM %s %d %s %s\n", id, seq, valor_str, cs);
        send(sock, buf, strlen(buf), 0);
        printf("-> %s", buf);

        n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("El servidor cerró la conexión\n");
            break;
        }
        buf[n] = '\0';
        printf("<- %s", buf);

        /* variar la lectura: entre -1.00 y +1.00 respecto de la anterior */
        valor += ((rand() % 201) - 100) / 100.0f;

        sleep(1);
    }

    close(sock);
    return 0;
}
