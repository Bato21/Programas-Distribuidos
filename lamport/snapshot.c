/* snapshot.c — snapshot consistente (Chandy-Lamport simplificado) */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>

#define NPROC 3
#define MAX_TRANSIT 32

typedef enum { TRANSFER, MARKER } MsgType;

typedef struct {
  MsgType type;
  int ts;       /* timestamp Lamport */
  int from;     /* pid del emisor */
  int amount;   /* solo para TRANSFER */
} Msg;

typedef struct {
  int clock;
  int pid;
  int counter;            /* estado local */
  int snap_counter;       /* estado capturado */
  int snap_taken;         /* ¿ya tomé mi snapshot? */
  int marker_from[NPROC]; /* ¿recibí MARKER de Pi? */
  int transit[NPROC][MAX_TRANSIT]; /* montos en tránsito por canal j → yo */
  int n_transit[NPROC];            /* cuántos hay en cada canal */
} Process;

int pipes[NPROC][NPROC][2]; /* pipes[i][j] = pipe de i a j */
int resultado[2];           /* pipe hijos → padre para verificar la suma */

// Estado del proceso actual (cada hijo tiene su propia copia después del fork)
Process p;

// Enviar es un evento: primero sube el reloj, después manda
void enviar(int dest, MsgType type, int amount){
  p.clock++;
  Msg m = {type, p.clock, p.pid, amount};
  write(pipes[p.pid][dest][1], &m, sizeof(Msg));
}

// Guardo mi estado y aviso a todos con un MARKER
void tomar_snapshot(void){
  p.snap_counter = p.counter;
  p.snap_taken = 1;
  printf("P%d toma snapshot: counter=%d ts=%d\n", p.pid, p.snap_counter, p.clock);
  for (int j = 0; j < NPROC; j++){
    if (j == p.pid) continue;
    enviar(j, MARKER, 0);
  }
}

void procesar(Msg m){
  // Lamport al recibir
  if (p.clock < m.ts) p.clock = m.ts + 1;
  else p.clock++;

  if (m.type == TRANSFER){
    // El estado real siempre se actualiza
    p.counter += m.amount;
    // Ya tomé mi snapshot pero el canal m.from sigue abierto: está en tránsito
    if (p.snap_taken && !p.marker_from[m.from]){
      p.transit[m.from][p.n_transit[m.from]] = m.amount;
      p.n_transit[m.from]++;
      printf("P%d registra en tránsito %d desde P%d\n", p.pid, m.amount, m.from);
    }
  }
  else if (m.type == MARKER){
    // Primer MARKER: tomo snapshot (y ese canal queda vacío)
    if (!p.snap_taken) tomar_snapshot();
    // Cierro el canal m.from → yo
    p.marker_from[m.from] = 1;
  }
}

// Terminé cuando tengo mi snapshot y el MARKER de todos los demás
int snapshot_completo(void){
  if (!p.snap_taken) return 0;
  for (int j = 0; j < NPROC; j++){
    if (j == p.pid) continue;
    if (!p.marker_from[j]) return 0;
  }
  return 1;
}

// Espera hasta 100 ms por un mensaje. Devuelve 1 si leyó uno, 0 si fue timeout
int recibir_uno(Msg *m){
  fd_set set;
  FD_ZERO(&set);
  int maxfd = 0;
  for (int j = 0; j < NPROC; j++){
    if (j == p.pid) continue;
    FD_SET(pipes[j][p.pid][0], &set);
    if (pipes[j][p.pid][0] > maxfd) maxfd = pipes[j][p.pid][0];
  }

  struct timeval tv = {0, 100000}; // 100 ms
  if (select(maxfd + 1, &set, NULL, NULL, &tv) <= 0) return 0;

  for (int j = 0; j < NPROC; j++){
    if (j == p.pid) continue;
    if (FD_ISSET(pipes[j][p.pid][0], &set)){
      read(pipes[j][p.pid][0], m, sizeof(Msg));
      return 1;
    }
  }
  return 0;
}

void hijo(int yo){
  p.pid = yo;
  p.counter = 100;
  srand(getpid());

  int ticks = 0; // cada tick son ~100 ms sin mensajes
  while (!snapshot_completo()){
    Msg m;
    if (recibir_uno(&m)){
      procesar(m);
      continue;
    }

    ticks++;
    // P0 inicia el snapshot después de ~1 s
    if (yo == 0 && !p.snap_taken && ticks >= 10) tomar_snapshot();

    // Antes de mi snapshot, transfiero a alguien al azar
    // (después dejo de transferir para no escribir a procesos que ya terminaron)
    if (!p.snap_taken){
      int dest = (yo + 1 + rand() % (NPROC - 1)) % NPROC;
      int amount = 1 + rand() % 20;
      if (p.counter >= amount){
        p.counter -= amount;
        enviar(dest, TRANSFER, amount);
      }
    }
  }

  // Resultado: estado capturado + todo lo que quedó en tránsito
  int total = p.snap_counter;
  printf("P%d: snap_counter=%d reloj=%d tránsito=[", p.pid, p.snap_counter, p.clock);
  for (int j = 0; j < NPROC; j++){
    for (int k = 0; k < p.n_transit[j]; k++){
      printf(" P%d→P%d:%d", j, yo, p.transit[j][k]);
      total += p.transit[j][k];
    }
  }
  printf(" ]\n");

  write(resultado[1], &total, sizeof(int));
}

int main(void){
  if (pipe(resultado) == -1){
    perror("pipe");
    return 1;
  }
  for (int a = 0; a < NPROC; a++){
    for (int b = 0; b < NPROC; b++){
      if (a == b) continue;
      if (pipe(pipes[a][b]) == -1){
        perror("pipe");
        return 1;
      }
    }
  }

  for (int i = 0; i < NPROC; i++){
    pid_t pid = fork();
    if (pid == -1){
      perror("fork");
      return 1;
    }
    if (pid == 0){
      // Cierro los extremos que no uso (igual que en el Ej 11)
      for (int a = 0; a < NPROC; a++){
        for (int b = 0; b < NPROC; b++){
          if (a == b) continue;
          if (a != i) close(pipes[a][b][1]);
          if (b != i) close(pipes[a][b][0]);
        }
      }
      close(resultado[0]);
      hijo(i);
      exit(0);
    }
  }

  // El padre no usa la malla
  for (int a = 0; a < NPROC; a++){
    for (int b = 0; b < NPROC; b++){
      if (a == b) continue;
      close(pipes[a][b][0]);
      close(pipes[a][b][1]);
    }
  }
  close(resultado[1]);

  // Sumo lo que reporta cada hijo
  int suma = 0, total;
  for (int i = 0; i < NPROC; i++){
    read(resultado[0], &total, sizeof(int));
    suma += total;
  }
  for (int i = 0; i < NPROC; i++) wait(NULL);

  printf("Suma del snapshot = %d %s\n", suma, suma == 100 * NPROC ? "(OK)" : "(ERROR)");
  return 0;
}
