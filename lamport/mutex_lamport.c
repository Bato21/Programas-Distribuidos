/* mutex_lamport.c — esqueleto */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/select.h>

#define NPROC 3
#define CICLOS 2

typedef enum { REQUEST, REPLY, RELEASE } MsgType;

typedef struct {
  MsgType type;
  int ts;    /* timestamp Lamport del emisor */
  int pid;   /* PID lógico del emisor */
} Msg;

typedef struct {
  int ts;
  int pid;
} Req;

typedef struct {
  Req items[NPROC]; // Arreglo de pedidos (Req) ordenados
  int n; // Numero de pedidos
}Cola;

// Inicializamos la cola
Cola cola = { .n = 0 }; 

// Toma 2 punteros de entrada
int req_cmp (const void *a, const void *b){
  // Los definimos como Req
  const Req *x = (const Req *)a;
  const Req *y = (const Req *)b; 

  // El primero es menor que el segundo
  if (x->ts < y->ts) return -2;
  // El segundo es menor que el primero
  else if (x->ts > y->ts) return 2;
  // Si los relojes son iguales desempate por id
  else{
    // Si el id de x es menor, entonces va primero
    if (x->pid < y->pid) return -2;
    // Si el id de y es menor, entonces va primero
    else if (x->pid > y->pid) return 2;
    else return 0;
  }
}

// Funcion para insertar un req en una cola
void cola_insertar(Cola *c, Req req){
  c->items[c->n] = req;
  c->n++;
  qsort(c->items, c->n, sizeof(Req), req_cmp);
}

// Funcion para sacar un req en una cola
void cola_quitar(Cola *c, int pid){
  // Variable para guardar posicion del pid
  int k = -1;
  // Recorremos la cola
  for(int i = 0; i < c->n; i++){
    // Cuando el valor checkeado corresponde al pid buscado
    if (c->items[i].pid == pid){
      // Guardamos la posicion
      k = i;
      break;
    }
  }
  // Si no se encuentra el PID en la cola terminamos la funcion
  if (k == -1) return;
  // Recorremos el arreglo de k en adelante y vamos desplazando a la izquierda
  for (; k < c->n - 1; k++) c->items[k] = c->items[k+1];
  c->n--;
}

int pipes[NPROC][NPROC][2]; /* pipes[i][j] = pipe de i a j */

// Inicializamos el reloj global en 0
int clock = 0;
// PID logico de cada proceso
int yo = 0;
// Contador global de respuestas
int replies = 0;
// Contador global de releases
int releases = 0;

// Funcion para enviar mensajes
void enviar(int dest, MsgType type, int ts){
  Msg mensaje = {type, ts, yo};
  write(pipes[yo][dest][1], &mensaje, sizeof(Msg));
}

// Funcion para procesar mensajes
void procesar(Msg r){
  // Ajustamos el reloj según lo recibido
  if (clock < r.ts) clock = r.ts +1;
  else clock ++;

  // Según el type ejecutamos las funciones
  if (r.type == REQUEST){ 
    // Creamos la estructura a enviar
    Req nuevo = {r.ts, r.pid};
    // Metemos el nuevo elemento a la cola
    cola_insertar(&cola, nuevo);
    clock++; // Antes de enviar aumentamos el reloj
    // Mandamos el mensaje
    enviar(r.pid, REPLY, clock);
  }

  // Si es una respuesta aumentamos el contador
  else if (r.type == REPLY) replies++;
  // Si es release eliminamos de la cola
  else if (r.type == RELEASE) {
    cola_quitar(&cola, r.pid);
    releases++;
  }
  // Si no es nada no hace nada;
  else return;
}

// Funcion para meter nuestro pedido a la cola
void request_cs(void){
  // Solicitar es una acción por lo que aumentamos el reloj
  clock++;
  // Metemos nuestra request a la cola
  Req propio = {clock, yo};
  cola_insertar(&cola, propio);
  // Reiniciamos las respuestas que recibimos a 0
  replies = 0;
  // Mandamos la solicitud a todos
  for(int j = 0; j < NPROC; j++){
    if (j == yo) continue;
    enviar(j, REQUEST, clock);
  }
}

// Funcion para eliminar nuestro pedido de la cola
void release_cs(void){
  // Sacamos nuestro pedido de la cola
  cola_quitar(&cola, yo);
  // Aumentamos el reloj antes de mandar
  clock++;
  // Mandamos el RELEASE a todos excepto a nosotros
  for (int j = 0; j < NPROC; j++){
    if(j == yo) continue;
    enviar(j, RELEASE, clock);
  }
}

// Funcion para entrar a la cola
int puedo_entrar(void){
  // Si tengo respuesta de todos y mi solicitud es la primera puedo entrar
  if (cola.n > 0 && 1 + replies == NPROC && cola.items[0].pid == yo) return 1;
  // Si no espero
  else return 0;
}

Msg recibir_uno(void){
  Msg m;
  fd_set set;
  FD_ZERO(&set);
  int maxfd = 0;

  for (int j = 0; j < NPROC; j++) {
    if (j == yo) continue; 
    // Agregar el pipe al ser
    FD_SET(pipes[j][yo][0], &set); 
    // Actualizar el maxfd si es necesario
    if (pipes[j][yo][0] > maxfd) maxfd = pipes[j][yo][0];
  }

  select(maxfd + 1, &set, NULL, NULL, NULL);

  for (int j = 0; j < NPROC; j++){
    if (j == yo) continue;
    if (FD_ISSET(pipes[j][yo][0], &set)){
      read(pipes[j][yo][0], &m, sizeof(Msg));
      return m;
    }
  }

  return m; 
}

void enter_cs(void){
  while(puedo_entrar() != 1){
    procesar(recibir_uno());
  }
}

int main(void) {
  /* TODO: diseñar e implementar */

  // Si fallan las pipes
  if (pipe(pipes[0][1]) == -1 || pipe(pipes[0][2]) == -1 ||
      pipe(pipes[1][0]) == -1 || pipe(pipes[1][2]) == -1 ||
      pipe(pipes[2][0]) == -1 || pipe(pipes[2][1]) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < NPROC; i++){
    pid_t pid = fork();
    if (pid == -1){
      perror("fork");
      return 1;
    }

    // Creamos a los hijos, P0, P1 y P2
    if (pid == 0){
      yo = i;
      for (int a = 0; a < NPROC; a++){
        for (int b = 0; b < NPROC; b++){
          if (a == b) continue; // No existe comunicación hacia uno mismo

          // Estamos en pipes[a][b], osea comunicación de a → b
          // Osea que a escribe y b escucha
          // Si no soy yo quien escribe, no necesito el extremo de escritura asi que lo cierro
          if (a != i){
            close(pipes[a][b][1]);
          }
          // Si no soy yo quien lee, no necesito el extremo de lectura asi que lo cierro
          if (b != i){
            close(pipes[a][b][0]);
          }
        }
      }

      // 2 ciclos 
      for (int ciclo = 0; ciclo < CICLOS; ciclo++){
        // Solicitamos acceso
        request_cs();
        // Esperamos a entrar
        enter_cs();
        printf("P%d ts=%d entra\n", yo, clock);
        // Escribimos en la sección crítica
        FILE *f = fopen("log.txt", "a");
        fprintf(f, "P%d ts=%d escribe\n", yo, clock);
        fclose(f);
        // Salimos
        release_cs();
        printf("P%d ts=%d sale\n", yo, clock);
      }
      // Mientras queden procesos pendientes de salir, procesamos los mensajes que nos lleguen
      while(releases < CICLOS * (NPROC - 1)){
        procesar(recibir_uno());
      }
      exit(0);
    }
  }

  // Cerramos las pipes que no usa el padre
  for (int a = 0; a < NPROC; a++){
    for (int b = 0; b < NPROC; b++){
      if( a == b) continue;
      close(pipes[a][b][0]);
      close(pipes[a][b][1]);
    }
  }

  // Esperamos a los hijos
  for (int i = 0; i < NPROC; i++) wait(NULL);
  
  return 0;
}
