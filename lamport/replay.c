/* replay.c */
#include <stdio.h>

typedef enum { LOCAL, SEND, RECV } EvType;

typedef struct {
  EvType type;
  int ts_recv;   /* válido solo si type == RECV */
} LogEntry;

LogEntry log_[] = {
  {LOCAL,0}, {LOCAL,0}, {SEND,0}, {RECV,7},
  {LOCAL,0}, {RECV,5}, {SEND,0}, {RECV,12},
};
#define N (sizeof(log_)/sizeof(log_[0]))

void replay(int *clock, LogEntry *log, int n) {
  /* Itera el log aplicando Lamport.
     Imprime "evento N: C=valor" por cada entrada. */
  /* TODO */
}

int main(void) {
  int clock = 0;
  replay(&clock, log_, N);
  return 0;
}
