#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     7070
#define BUF_SIZE 256

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

    /* TODO B.1 */

    // Creamos el socket de conexión de la sonda, solo hay 1 contrariamnete al servidor
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0){ 
        perror("socket");
        return 1;
    }

    printf("sock = %d\n", sock);

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
    };

    // La sonda necesita saber exactamente a que ip llamar
    // inet_pton pasa un string a una direccion válida
    // Se le pasa el formato, el texto a transformar y luego donde guardar la dirección trasnformada
    if (inet_pton(AF_INET, ip, &srv.sin_addr) <= 0){
        perror("inet_pton");
        return 1;
    }

    // Le 'marcamos' a la dirección con
    // Socket de origen, la dirección con & para saber donde está (NOC CAMBIA), tamaño de la dirección (NO CAMBIA)
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0){
        perror("connect");
        return 1;
    }

    printf("Conecta a la base\n");

    /* TODO B.2 */

    // Mensaje que envíamos al servidor
    char buf[BUF_SIZE];

    // Armamos el mensaje que vamos a enviar y lo enviamos
    snprintf(buf, sizeof(buf), "CONNECT %s\n", id);
    send(sock, buf, strlen(buf), 0);

    // Mensaje que recibimos del servidor viene con un \0 al final
    char resp[BUF_SIZE];

    // Recibimos el mensaje y guardamos el numero de caracteres que tiene sin el \0
    int n = recv(sock, resp, sizeof(resp) - 1, 0);

    // Si no recibimos nada
    if (n <= 0) {
        printf("ERROR: No se recibió respuesta del servidor\n");
        return 1;
    }

    printf("Llegaron %d bytes\n", n);

    // Limpiamos el mensaje recibido
    resp[n] = '\0';
    resp[strcspn(resp, "\r\n")] = '\0';
    printf("Mensaje: [%s]\n", resp);

    // El mensaje puede ser HELLO id o REJECT id
    // Así que tenemos que sacar la primera palabra y ver cual de las 2 es
    // Variable en la que almacenaremos la primera palabra
    char tipo[16];
    // Sacamos la primera palabra de la respuesta y se la asignamos a la variable
    sscanf(resp, "%7s", tipo);
    printf("Tipo = %s\n", tipo);

    if (strcmp(tipo, "HELLO") != 0){
        printf("ERROR: Conexión rechazada\n");
        close(sock);
        return 1;
    }







    

    



    /* TODO B.3 */

    return 0;
}
