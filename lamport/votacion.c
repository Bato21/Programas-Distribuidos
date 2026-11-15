  /* votacion.c — práctica: socket + fork + pipe + select (sin mirar el Ej 12)
 *
 * Servidor de votación en el puerto 9191. Hay 4 opciones (0, 1, 2, 3).
 * Cada cliente se conecta y envía líneas con el formato:
 *
 *     VOTO <id_cliente> <opcion>\n        ej: "VOTO 7 2"
 *
 * 1. El padre acepta conexiones y hace fork() por cada cliente.
 * 2. El hijo recibe líneas en un while(recv > 0), las parsea con sscanf
 *    y, si la opción es válida (0..3), manda al padre SOLO el int opcion
 *    por una pipe hijo → padre. Si la línea es inválida, la ignora.
 * 3. El padre usa select() con timeout de 1 s sobre:
 *      - el socket del servidor  (llega un cliente nuevo → accept + fork)
 *      - la pipe                  (llega un voto → votos[opcion]++)
 * 4. Cada 3 segundos el padre imprime el conteo:
 *      Conteo: [0]=4 [1]=1 [2]=7 [3]=0
 *
 * Sin Lamport, sin qsort, sin structs: solo el esqueleto de red.
 * Escríbelo de memoria. Si te pierdes, anota la duda en un comentario y sigue.
 */

/* TODO: includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

/* TODO: defines (PORT, MAX_BUF, NOPC) */
#define PORT 9000
#define MAX_BUF 64
#define NOPC 4

/* TODO: int votos[NOPC]; */
int votos[NOPC];

/* TODO: void handle_cliente(int fd, int pipe_w) */
void handle_cliente(int fd, int pipe_w){
  // Mensaje recibido
  char buf[MAX_BUF]; 
  
  int n;
  // Bucle de lectura
  while((n = recv(fd, buf, sizeof(buf) - 1, 0)) > 0){

    // Variable voto
    int voto;
    int id;

    buf[n] = '\0';
    buf[strcspn(buf, "\n\r")] = '\0';
    printf("Mensaje recibido: %s", buf);

    // Guardamos las variables
    if (sscanf(buf, "VOTO %d %d", &id, &voto) != 2) continue;
    // Si el voto es invalido
    if (voto < 0 || voto >= NOPC) continue;

    // Mandamos el voto por pipe
    write(pipe_w, &voto, sizeof(voto));
  }
  close(fd);
} 

int main(void){
  /* TODO */
  signal(SIGCHLD, SIG_IGN);
  // Pipe entre padre e hijo
  int pipe_fd[2];
  if(pipe(pipe_fd) < 0){
    perror("pipe");
    return 1;
  }

  // Creamos el socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0){
    perror("socket");
    return 1;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
    .sin_addr.s_addr = INADDR_ANY,
  };

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
    perror("bind");
    return 1;
  }

  if(listen(server_fd, 10) < 0){
    perror("listen");
    return 1;
  }

  time_t ultimo = time(NULL);

  while (1){
    fd_set set;
    FD_ZERO(&set);
    FD_SET(server_fd, &set);
    FD_SET(pipe_fd[0], &set);
    // Sacamos el máximo
    int max_fd = (server_fd > pipe_fd[0] ? server_fd : pipe_fd[0]); 
    // Esperamos 5 segundos
    struct timeval tv = {1, 0};
    // Esperamos algo de los fd
    select(max_fd + 1, &set, NULL, NULL, &tv);
    // Si recibo algo del servidor
    if(FD_ISSET(server_fd, &set)){
      int client_fd = accept(server_fd, NULL, NULL);
      if (client_fd < 0){
        perror("accept");
        return 1;
      }

      pid_t pid = fork();

      if (pid < 0){
        perror("fork");
        return 1;
      }

      // Proceso hijo
      else if (pid == 0){
        close(server_fd);
        // Cerramos lectura que no usamos
        close(pipe_fd[0]);
        handle_cliente(client_fd, pipe_fd[1]);
        exit(0);
      }

      // Proceso padre
      else if(pid > 0){
        close(client_fd);
      }
    }

    // Si recibo algo del pipe del hijo
    else if(FD_ISSET(pipe_fd[0], &set)){
      int voto;
      read(pipe_fd[0], &voto, sizeof(voto));
      votos[voto] += 1;
    }
    if (time(NULL) - ultimo >= 3){
      for (int i=0; i < NOPC; i++){
        printf("Votos para la opción %d: %d\n", i, votos[i]);
      }
      ultimo = time(NULL);
    }
  }

  close(server_fd);
  return 0;
}
