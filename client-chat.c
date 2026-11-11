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
 *  Ejecutar:  ./client Camila
 *             ./client Camila 192.168.1.5   (otra IP)
 *
 * ============================================================
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

#define PORT     8080
#define BUF_SIZE 512

// Reloj del proceso
int reloj = 0;

int main(int argc, char *argv[]) {

    /* argv[0] = "./client", argv[1] = nombre, argv[2] = IP (opcional) */
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <nombre> [ip]\n", argv[0]);
        return 1;
    }
    const char *nombre    = argv[1];
    const char *server_ip = (argc > 2) ? argv[2] : "127.0.0.1";

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
    
    printf("Conectado a %s:%d como %s\n", server_ip, PORT, nombre);
    printf("Escribe mensajes (QUIT para salir).\n");
    printf("El chat arranca cuando se conecten todos los clientes.\n\n");

    fflush(stdout);   /* vaciar el buffer antes de fork(), si no sale duplicado */

    // Variable para almacenar mensaje
    char buf[BUF_SIZE];
    // File descriptor del teclado
    int teclado_fd = 0;

    // Bucle para enviar o recibir mensajes
    while(1){
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock_fd, &set);
        FD_SET(teclado_fd, &set);
        // Hasta donde revisa el select
        int max_fd = (sock_fd > teclado_fd ? sock_fd : teclado_fd);
        // Si hay que esperar
        // struct timeval tv = {1, 0};
        // Esperamos que llegue algo sin timeout por consulta
        select(max_fd + 1, &set, NULL, NULL, NULL);
        // Si llega un mensaje
        if(FD_ISSET(sock_fd, &set)){
            char respuesta[BUF_SIZE];
            char mensaje[BUF_SIZE];
            char id[BUF_SIZE];
            int T;
            int n = recv(sock_fd, respuesta, sizeof(respuesta) - 1, 0);
            if (n <= 0){ 
                printf("\nServidor cerró la conexión\n"); 
                break;
            }
            respuesta[n] = '\0';
            sscanf(respuesta, "[T=%d] %31[^:]: %[^\n]", &T, id, mensaje);
            if (T > reloj) reloj = T + 1;
            else reloj++;
            printf("[T=%d] %s: %s\n", T, id, mensaje);
            fflush(stdout);
        }

        // Si llega algo del teclado
        if(FD_ISSET(teclado_fd, &set)){
            if( fgets(buf, sizeof(buf), stdin) == NULL) break;
            buf[strcspn(buf, "\n")] = '\0'; // Sacamos el \n del final
            // Usuario escribe QUIT para salir
            if (strcmp(buf, "QUIT") == 0) break;
            // Ignoramos un mensaje vacío
            if (strlen(buf) == 0) continue;
            // Mensaje a mandar
            char to_send[BUF_SIZE + 64];
            reloj++;
            snprintf(to_send, sizeof(to_send), "[T=%d] %s: %s\n", reloj, nombre, buf);
            // Si falla el send
            if (send(sock_fd, to_send, strlen(to_send), 0) < 0){perror("send"); break;}
        }
    }

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
