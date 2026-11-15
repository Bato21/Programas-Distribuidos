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

    /* TODO B.2 */

    /* TODO B.3 */

    return 0;
}
