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
  // Recorremos el arreglo log
  for (int i = 0; i < n; i++){  
    // Si el evento es RECV
    if (log[i].ts_recv > 0){
      // Si el reloj local es más grande
      if (log[i].ts_recv < *clock){
        (*clock)++;
      }
      
      // Si no o el local es más chico o son iguales
      else{
        *clock = log[i].ts_recv + 1;
      }
    }

    // Si es LOCAL o SEND
    else {
      // Sumamos 1 al reloj
      (*clock)++;
    }

    // Imprimimos el evento
    printf("Evento %d: C=%d\n", i+1, *clock);
  }
}

int main(void) {
  int clock = 0;
  replay(&clock, log_, N);
  return 0;
}
