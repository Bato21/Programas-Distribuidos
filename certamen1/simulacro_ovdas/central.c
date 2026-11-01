/*
 * SIMULACRO CERTAMEN - CENTRAL OVDAS (servidor TCP)   -- esqueleto
 * Compilar: gcc -Wall -o central central.c
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

void handle_estacion(int fd) {
    char buf[BUF_SIZE];

    /* TODO A.1 */

    int n = recv(fd, buf, strlen(buf), 0);

    char check[16];

    if (n < 0){
        perror("recv");
        exit(EXIT_FAILURE);
    }

    buf[n] = '\0';

    printf("Mensaje recivido: %s", buf);

    sscanf(buf, "");

    /* TODO A.2 */
    // Mientras siga recibiendo lecturas
    while(n > 0){
        
    }

}

int main(void) {
    /* TODO A.3 */

    // Creamos socket

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Inicializamos direccion

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    // de '7000' a la direccion exacta del puerto
    addr.sin_port = htons(PORT);
    // Modo promiscuo
    addr.sin_addr.s_addr = INADDR_ANY;


    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while(1){
        // Conexion con cliente
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0){
            perror("accept");
            // Sigo esperando conexiones
            continue;
        }

        pid_t pid = fork();

        // Error fork

        // Proceso hijo
        if (pid == 0){
            close(server_fd);
            handle_estacion(client_fd);
            close(client_fd);
            exit(EXIT_SUCCESS);
        }

        // Proceso padre
        else{
            close(client_fd);
            // Matar procesos zombies
            waitpid(-1, NULL, WNOHANG);
        }
        
    }
    
    close(server_fd);
    return 0;
}
