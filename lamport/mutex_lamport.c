/* mutex_lamport.c — esqueleto */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define NPROC 3

typedef enum { REQUEST, REPLY, RELEASE } MsgType;

typedef struct {
  MsgType type;
  int ts;    /* timestamp Lamport del emisor */
  int pid;   /* PID lógico del emisor */
} Msg;

/*
 * pipes[i][j] = fd para que proceso i ESCRIBA al proceso j.
 * 1. Crear las pipes antes de fork().
 * 2. Después de fork(), cerrar los extremos que no usa cada proceso.
 * 3. Implementar request_cs(), enter_cs(), release_cs().
 * 4. Cada proceso: 2 ciclos de request → enter → write → release.
 */

int main(void) {
  /* TODO: diseñar e implementar */
  return 0;
}
