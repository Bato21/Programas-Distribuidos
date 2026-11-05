#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Librería de tiempo para la aleatoriedad
#include <time.h>

#define PORT     9090
#define BUF_SIZE 256

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: ./sonda <ARES-N> <IP> <valor_inicial>\n");
        return 1;
    }
    int id = atoi(argv[1]);    
    const char *ip    = argv[2];
    int valor = atoi(argv[3]);

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


    /* TODO B.3 */
    // Bucle de telemetría

    int reloj = 0;
    for (int seq = 1; seq < 9; seq++){

        // Mensaje a enviar 
        char env[BUF_SIZE];  

        // Actualizamos el reloj
        reloj++;
        snprintf(env, sizeof(env), "TELEM %d %d %d %d\n", id, seq, valor, reloj);
        printf("Enviamos: %s", env);

        // Mandamos las lecturas
        send(sock, env, strlen(env), 0);

        // Cambiamos el valor enviado
        valor += 5;

        //Agregamos el sleep al final
        sleep(1);
    }

    close(sock);
    return 0;
}
