/*
 * ============================================================
 *  CHAT DISTRIBUIDO - Versión 2: Cliente full-duplex con fork
 * ============================================================
 *
 *  El problema de la versión 1:
 *    El bucle era por turnos -> fgets() bloqueaba esperando el
 *    teclado, así que si otro cliente escribía, no lo veías hasta
 *    apretar Enter. Inservible para un chat.
 *
 *  La solución: fork(). Dos procesos que comparten el MISMO socket
 *  (fork copia la tabla de descriptores, igual que en ejemploPipe.c):
 *
 *      hijo  : teclado  -> send(socket)
 *      padre : recv(socket) -> pantalla
 *
 *  Acá NO hace falta pipe: el socket ya es bidireccional y cada
 *  proceso usa una dirección.
 *
 *  Compilar:  gcc -Wall -Wextra -g -o client client-chat.c
 *  Ejecutar:  ./client
 *             ./client 192.168.1.5   (otra IP)
 *
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     8080
#define BUF_SIZE 512

int main(int argc, char *argv[]) {

    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); return 1; }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(PORT)
    };

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "IP inválida: %s\n", server_ip);
        return 1;
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "¿Está corriendo ./server?\n");
        return 1;
    }

    printf("Conectado a %s:%d\n", server_ip, PORT);
    printf("Escribe mensajes (QUIT para salir).\n");
    printf("El chat arranca cuando se conecten todos los clientes.\n\n");

    fflush(stdout);   /* vaciar el buffer antes de fork(), si no sale duplicado */

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return 1; }

    if (pid == 0) {
        /* ===== HIJO: teclado -> socket ===== */
        char input[BUF_SIZE];

        while (fgets(input, sizeof(input), stdin) != NULL) {

            input[strcspn(input, "\n")] = '\0';   /* quitar '\n' final */
            if (strlen(input) == 0) continue;     /* ignorar líneas vacías */

            /* el '\n' hace de delimitador de mensaje para el servidor */
            char to_send[BUF_SIZE + 2];
            snprintf(to_send, sizeof(to_send), "%s\n", input);

            if (send(sock_fd, to_send, strlen(to_send), 0) < 0) {
                perror("send");
                break;
            }

            if (strcmp(input, "QUIT") == 0) break;
        }

        exit(0);   /* el padre se encarga de cerrar el socket */
    }

    /* ===== PADRE: socket -> pantalla ===== */
    char respuesta[BUF_SIZE];

    while (1) {
        int n = recv(sock_fd, respuesta, sizeof(respuesta) - 1, 0);
        if (n <= 0) {
            printf("\nServidor cerró la conexión.\n");
            break;
        }
        respuesta[n] = '\0';   /* recv tampoco pone el '\0' */
        printf("%s", respuesta);
        fflush(stdout);
    }

    /* el hijo puede estar bloqueado en fgets() esperando teclado: lo cortamos */
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);

    close(sock_fd);
    printf("Desconectado.\n");
    return 0;
}


/* A implementar:

- Suplantacion de identidad: Un cliente envía JOIN Alice siendo Bob -> todos los demás clientes creen que es Alice.
- Replay attack: Se captura un paquete JOIN admin válido y se reenvía para suplantar la identidad de admin.
- Sniffing: El tráfico TCP viaja en texto plano, por lo que un atacante puede leerlo y obtener información sensible.
- Mensaje malicioso: Un cliente envía un mensaje con contenido malicioso, por ejemplo, 50.000 bytes de datos, el servidor hace recv(buf, 1024) pero no valida el tamaño del mensaje, por lo que se produce un buffer overflow y el servidor se cae.

*/
