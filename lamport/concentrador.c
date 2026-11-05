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
#include <signal.h>

/* concentrador.c — esqueleto */
#define PORT 9090
#define MAX_BUF 128
#define MAX_PEND 64

typedef struct {
  int ts;
  int pid;              /* id numérico de la sonda */
  char line[MAX_BUF];   /* línea completa para el log */
} PendingEvent;

// Relok global
int reloj = 0;
PendingEvent pend[MAX_PEND];   // eventos esperando ser escritos
int n_pend = 0;                // cuántos hay

/*
 * 1. Acepta conexiones con fork() (como servidor.c del Control 01).
 * 2. Cada hijo recibe mensajes, los parsea y extrae el ts.
 * 3. PROBLEMA: los hijos no pueden coordinar orden causal sin IPC.
 *    Diseña un mecanismo para que el padre (o un proceso dedicado)
 *    recolecte los eventos y los ordene antes de escribirlos.
 *    Opciones: pipe hijo→padre, shared memory + semáforo,
 *    cola de mensajes (msgget/msgsnd).
 * 4. El concentrador mantiene su propio reloj Lamport.
 * 5. Cada 2 segundos flushea el buffer al log, ordenado por (ts, pid).
 */


// Funcón para trabajar las sondas que vayan siendo aceptadas
void handle_sonda(int fd, int pipe_w){

  char buf[MAX_BUF];
  int n;
  while((n = recv(fd, buf, sizeof(buf) - 1, 0)) > 0){
    // Añadimos caracter de fin al mensaje y lo imprimimos
    buf[n] = '\0';
    buf[strcspn(buf, "\n\r")] = '\0';
    printf("Mensaje recibido: [%s]\n", buf);

    // Parseamos el mensaje de llegada
    char tipo[6]; // TELEM o RELAY
    int id; // Id de la sonda
    int dest; // Id destino si es RELAY, seq si es TELEM pero ambos id
    int val; // Igual para ambos, valor
    int ts; // Igual para ambos, reloj 

    if (sscanf(buf, "%5s %d %d %d %d", tipo, &id, &dest, &val, &ts) != 5) continue;

    // Armamos el evento y lo escribimos en la pipe
    PendingEvent evento;
    evento.ts = ts;
    evento.pid = id;
    strcpy(evento.line, buf);
    write(pipe_w, &evento, sizeof(PendingEvent));
  }

  close(fd);
}

int pend_cmp(const void *a, const void *b){
  const PendingEvent *x = (const PendingEvent *)a;
  const PendingEvent *y = (const PendingEvent *)b;

  // Si el primero es menor que el segundo
  if (x->ts < y->ts) return -2;
  // Si el segundo es menor que el primero
  else if (y->ts < x->ts) return 2;
  // Si son iguales
  else{
    // Si el id del primero es menor
    if (x->pid < y->pid) return -2;
    // Si el id del segundo es menor
    else if (y->pid < x->pid) return 2;
    // Si no error
    else return 0;
  }
}

 int main(void) {
  signal(SIGCHLD, SIG_IGN);   // el sistema limpia solo a los hijos que terminan

  // Creamos la pipe de comunicación sonda <-> servidor
  int pipe_fd[2];
  if (pipe(pipe_fd) == -1){
    perror("pipe");
    return 1;
  }

  // Le asignamos un socket al servidor
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd < 0){
    perror("socket");
    return 1;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
    .sin_addr.s_addr = INADDR_ANY
  };

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
    perror("bind");
    return 1;
  }

  // Dejamos al servidor escuchando hasta 10 conexiones
  if (listen(server_fd, 10) < 0){
    perror("listen");
    return 1;
  }

  time_t ultimo_flush = time(NULL);   // Momento de la ultima limpieza

  // Bucle para aceptar sondas
  while(1){

    // Inicializamos el set en 0
    fd_set set;
    FD_ZERO(&set);
    // Agregamos los 2 file descriptors que vamos a vigilar
    FD_SET(server_fd, &set);
    FD_SET(pipe_fd[0], &set);
    // Buscamos el más grande de los 2
    int maxfd = (server_fd > pipe_fd[0]) ? server_fd : pipe_fd[0];
    // Armamos el timeout de 1 segundo
    struct timeval tv = {1, 0}; // 1 segundo, 0 microsegundos
    // Esperamos a que un fd tenga algo o que se acabe el tiempo
    select(maxfd + 1, &set, NULL, NULL, &tv);
    // Si es que el llega un cliente (sonda) al fd
    if (FD_ISSET(server_fd, &set)){
      // Llega una sonda y le abrimos una linea de comunicacion
      int client_fd = accept(server_fd, NULL, NULL);
      if (client_fd < 0){
        perror("accept");
        return 1;
      }

      // Hacemos fork para poder manejar la sonda
      pid_t pid = fork();

      if (pid < 0){
        perror("fork");
        return 1;
      }

      // Proceso hijo
      else if(pid == 0){
        close(server_fd);
        // El hijo solo escribe por la pipe por lo que cerramos lectura
        close(pipe_fd[0]);
        handle_sonda(client_fd, pipe_fd[1]);
        exit(0);
      }

      // Proceso padre
      else if(pid > 0){
        // Cerramos al cliente
        close(client_fd);

      }
    }

    // Si es que llega algo al fd de la pipe (mensaje del hijo)
    if (FD_ISSET(pipe_fd[0], &set)){
      PendingEvent evento;
      read(pipe_fd[0], &evento, sizeof(PendingEvent));
      if(reloj < evento.ts) reloj = evento.ts + 1;
      else reloj++; 
      if (n_pend < MAX_PEND){ 
        pend[n_pend] = evento;
        n_pend++; 
      }

    }

    if (time(NULL) - ultimo_flush >= 2){
      // Ordenamos el buffer
      qsort(pend, n_pend, sizeof(PendingEvent), pend_cmp);
      // Abrimos el log
      FILE *f = fopen("registro.log", "a");
      // Escribimos los eventos ya ordenados
      for (int k = 0; k < n_pend; k++){
        fprintf(f, "%s\n", pend[k].line);
      }
      // Cerramos el archivo
      fclose(f);
      // Vaciamos el buffer
      n_pend = 0;
      // Reiniciamos el ultimo contador del tiempo
      ultimo_flush = time(NULL);
    }
  }

  // Cerramos el servidor 
  close(server_fd);
  return 0;
 }