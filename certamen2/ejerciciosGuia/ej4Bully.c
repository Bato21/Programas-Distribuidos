#include <stdio.h>
#include <stdlib.h>

#define N 5
#define CPU_MAX 90
#define CPU_MIN 40
#define LAT_MAX 80
#define LAT_MIN 15
#define UPT_MAX 500
#define UPT_MIN 50

typedef enum { ACTIVO, CAIDO } Estado;
typedef enum { ELECTION, OK, COORDINATOR } TipoMsg;
typedef struct { int id; double cpu, lat, uptime, aptitud; Estado estado; } Nodo;
typedef struct { TipoMsg tipo; int origen, destino; } Mensaje;

int enviar(Nodo red[], Mensaje m);   /* imprime y devuelve 1 si el destino está ACTIVO */


Nodo nodos[N + 1] = {
    {0, 0, 0, 0, CAIDO},
    {1, 70, 40, 120, ACTIVO},
    {2, 55, 15, 300, ACTIVO},
    {3, 90, 80, 50, ACTIVO},
    {4, 40, 25, 500, ACTIVO},
    {5, 80, 30, 200, CAIDO},
};

int apt[N + 1];

double normalizar(double x, double max, double min){ return ((x - min) / (max - min)); }

double F (double cpu, double lat, double upt){
    return ((0.5 * normalizar(cpu, CPU_MAX, CPU_MIN)) 
    + (0.3 * (1 - normalizar(lat, LAT_MAX, LAT_MIN))) 
    + (0.2 * normalizar(upt, UPT_MAX, UPT_MIN)));
}


int cmp(const void *a, const void *b) {
    int x = *(const int *)a;          /* id del nodo a */
    int y = *(const int *)b;          /* id del nodo b */
    if (nodos[x].aptitud < nodos[y].aptitud) return 1;   /* mayor primero */
    if (nodos[x].aptitud > nodos[y].aptitud) return -1;
    return 0;
}

int elecciones(int yo){
    int cand = yo;
}


int main(void){

    int orden[N + 1];
    for (int i = 1; i <= N; i++) {
        nodos[i].aptitud = F(nodos[i].cpu, nodos[i].lat, nodos[i].uptime); /* Rellenamos valores*/
        orden[i] = i;        /* ids, no valores */
    }

    qsort(orden + 1, N, sizeof(int), cmp);            /* +1: saltamos el índice 0 */

    for (int k = 1; k <= N; k++)
        printf("N%d: %.4f\n", orden[k]);



}