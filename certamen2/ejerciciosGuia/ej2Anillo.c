#include <stdio.h>
#include <stdlib.h>

#define N 6
#define OPERATIVO 1
#define INOPERATIVO 0

typedef struct Dron {
    int id;
    float bat;
    float dist;
    float vis;
    int estado;
    float apt;
} Dron;

float aptitud(float vis, float dist, float bat){
    float apt = 0;
    apt += vis;
    apt += (bat * 0.5);
    if(dist <= 0.5) apt += 100;
    else if(dist < 1.5) apt += 50;
    else apt += 20;
    return apt;
}

// Lista de drones
Dron drones[N + 1] = {
    {0, 0, 0, 0, INOPERATIVO, 0},
    {1, 78, 1.2, 85, OPERATIVO, 0},
    {2, 45, 0.8, 92, OPERATIVO, 0},
    {3, 0, 0, 0, INOPERATIVO, 0},
    {4, 91, 2.1, 70, OPERATIVO, 0},
    {5, 62, 0.5, 88, OPERATIVO, 0},
    {6, 83, 1.5, 45, OPERATIVO, 0}
};

int comparar(const void *a, const void *b){
    const Dron *x = a;
    const Dron *y = b;
    if (x->apt < y->apt) return 1;
    if (x->apt > y->apt) return -1;
    return 0;
}

int eleccion (int yo){
    int cand = yo;
    int actual = yo;
    do {
        int sig = actual % N + 1;
        while(drones[sig].estado == INOPERATIVO){
            printf("C%d -> C%d ELECTION (sin respuesta)\n", actual, sig);
            sig = sig % N + 1;
        }

        printf("C%d -> C%d ELECTION (cand: C%d)\n", actual, sig, cand);
        actual = sig;

        if(actual != yo){
            // Si la aptitud del siguiente es mejor que la del candidato la actualizamos
            if (drones[actual].apt > drones[cand].apt){
                cand = actual;
            }
            else continue;
        }
    }

    while (actual != yo);
    
    actual = yo;
    do {
        int sig = actual % N + 1;
        while(drones[sig].estado == INOPERATIVO){
            printf("C%d -> C%d ELECTION (sin respuesta)\n", actual, sig);
            sig = sig % N + 1;
        }

        printf("C%d -> C%d COORDINATOR C%d\n", actual, sig, cand);
        actual = sig;
    }

    while(actual != yo);

    return cand;
}

int main(){


    // Rellenamos las aptitudes
    for (int i = 1; i <= N; i++){
        if (drones[i].estado == OPERATIVO) {
            drones[i].apt = aptitud(drones[i].vis, drones[i].dist, drones[i].bat);
        }
    }   

    Dron orden[N + 1];
    for (int i = 0; i <= N; i++) orden[i] = drones[i];
    size_t n = N;
    qsort(orden + 1, n, sizeof(orden[0]), comparar);

    printf("Orden por aptitud:\n");
    for (size_t i = 1; i <= n; i++) {
        if (orden[i].estado == OPERATIVO) {
            printf("C%d (%.2f)\n", orden[i].id, orden[i].apt);
        }
    }
    printf("\n");

    int lider = eleccion(1);
    printf("Nuevo líder: C%d\n", lider);
    return 0;
}