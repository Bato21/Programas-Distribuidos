/* order.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char proc[4];
  int ts;
  char name[8];
} Event;

// a y b son punteros, no se definen aún como eventos, como son void, 
// No sabemos a que corresponden exactamente, por lo que no podemos llamar a->ts
int total_order_cmp(const void *a, const void *b) {
  /* TODO: <0 si a va primero, >0 si b va primero */

  //Creamos un evento x e y a partir de a y b
  // (const Event *): es trata a lo que venga despues de la expreción como un event o struct definido
  const Event *x = (const Event *)a;
  const Event *y = (const Event *)b;

  // Sacamos los ids de los relojes en una variable
  int id_a;
  sscanf(x->proc, "P%d", &id_a);
  int id_b;
  sscanf(y->proc, "P%d", &id_b);

  // Si el contador de x es más chico que el de y
  // Entonces a viene antes que b y devolvemos un negativo
  if (x->ts < y->ts) return -2;

  // Si el contador de y es más chico que el de x
  // Entonces b viene antes que a y devolvemos un positivo
  else if (x->ts > y->ts) return 2;

  // Si son iguales revisamos su proc (Pxxx)
  else {
    // Si el id de a es más chico que el de b viene antes a
    if (id_a < id_b) return -2;
    // Si el id de a es más grande que el de b viene despueś a
    else if(id_a > id_b) return 2;
    // Si son iguales error?
    else return 0;
  }

}


int main() {

  // Arreglo de 8 eventos, cada uno es {proc, ts, name}
  Event eventos[] = {
    {"P2", 4, "α"},
    {"P1", 4, "β"},
    {"P3", 1, "γ"},
    {"P1", 7, "δ"},
    {"P3", 4, "ε"},
    {"P2", 9, "ζ"},
    {"P1", 2, "η"},
    {"P3", 6, "θ"},
  };

  // Cantidad de eventos = tamaño total del arreglo / tamaño de un evento
  int n = sizeof(eventos) / sizeof(Event);

  qsort(eventos, n, sizeof(Event), total_order_cmp);

  // Desde i = 0, hasta n el tamaño del arreglo
  for (int i = 0; i<n; i++){
    printf("[%s ts=%d] %s\n", eventos[i].proc, eventos[i].ts, eventos[i].name);
  }

}