#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUF_SIZE 256
#define N 5
#define VIVOS 4

int fd[N+1];

int main(void){

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0){
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) < 0){
        perror("listen");
        return 1;
    }

    printf("Estacion costera escuchando conexiones en el puertp %d...\n", PORT);

    // Bucle para aceptar conexiones de los clientes
    for (int i = 1; i <= VIVOS; i++){
        int client_fd = accept(server_fd, NULL, NULL);
        char recibido[BUF_SIZE];
        int n = recv(client_fd, recibido, sizeof(recibido), 0);
        if(n <= 0){ perror("recv"); continue;}
        int id;
        sscanf(recibido, "CONNECT F%d\n", &id);
        // Guardamos el fd del cliente en la lista
        fd[id] = client_fd;
    }

    for(int i = 1; i <= N; i++){
        if (fd[i] == 0){
            printf("Faro F%d no responde...\n", i);
            continue;
        }

        pid_t pid = fork();

        if (pid < 0){ perror("fork"); continue;}

        // Proceso hijo
        if(pid == 0){
            close(server_fd);

            char recibido[BUF_SIZE];
            int n;
            // Mientras recibamos mensajes
            while((n = recv(fd[i], recibido, sizeof(recibido), 0)) > 0){
                if (n <= 0){
                    printf("Faro F%d cerró la conexión...\n", i);
                    close(fd[i]);
                    exit(0);
                }
                // formato mensajes: <destino> <resto>
                int destino;
                char resto[BUF_SIZE] = {0};
                sscanf(recibido, "%d %[^\n]", &destino, resto);

                if(fd[destino] == 0){
                    printf("[COSTERA] F%d -> F%d %s (F%d no contesta...)\n", i, destino, resto, destino);
                    continue;
                }

                send(fd[destino], resto, sizeof(resto), 0);
                
            }
            
            exit(0);
        }

        // Proceso padre
        if(pid > 0){

        }
    }

    close(server_fd);
    while(wait(NULL) > 0);
    exit(0);
}