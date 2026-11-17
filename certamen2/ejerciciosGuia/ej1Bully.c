#include <stdio.h>
#include <stdlib.h>

typedef struct Dron {
    int id;
    float bat;
    float dist;
    float vis;
    int estado;
    float apt;
} Dron;

#define OPERATIVO 1
#define INOPERATIVO 0
#define N_DRONES 6

float aptitud(float vis, float dist, float bat){
    float apt = 0;

    apt += vis;
    apt += (bat * 0.5);
    if( dist <= 0.5){
        apt += 100;
    }
    else if( dist < 1.50){
        apt += 50;
    }
    else apt += 20;

    return apt;
}

int comparar(const void *a, const void *b){
    const Dron *x = a;
    const Dron *y = b;
    if (x->apt < y->apt) return 1;
    if (x->apt > y->apt) return -1;
    return 0;
}

// Lista de drones
Dron drones[N_DRONES + 1] = {
    {0, 0, 0, 0, INOPERATIVO, 0},
    {1, 78, 1.2, 85, OPERATIVO, 0},
    {2, 45, 0.8, 92, OPERATIVO, 0},
    {3, 0, 0, 0, INOPERATIVO, 0},
    {4, 91, 2.1, 70, OPERATIVO, 0},
    {5, 62, 0.5, 88, OPERATIVO, 0},
    {6, 83, 1.5, 45, OPERATIVO, 0}
};

int eleccion(int yo){
    printf("Eleccion iniciada por C%d\n", yo);
    int respondio = 0;

    for (int i = 1; i <= N_DRONES; i++){
        // Si no soy yo y tienen mejor aptitud le pregunto
        if(i != yo && drones[i].apt > drones[yo].apt){
            printf("C%d -> C%d: ELECTION\n", yo, i);
            // Si esta vivo me responde y gana
            if (drones[i].estado == OPERATIVO){
                printf("C%d -> C%d OK\n", i, yo);
                respondio = i;
                break;
            }
        }
    }

    // Si nadie responde gano yo
    if(respondio == 0){
        for (int i = 1; i <= N_DRONES; i++){
            // Si no soy yo y está vivo le aviso que paso a ser lider
            if (i != yo && drones[i].estado == OPERATIVO){
                printf("C%d -> C%d : COORDINATOR\n", yo, i);
            }
        }
        return yo;
    }

    // Si llega hasta acá alguien respondió y pasa a hacer su eleccion
    return eleccion(respondio);
}


int main(){


    // Rellenamos las aptitudes
    for (int i = 1; i <= N_DRONES; i++){
        if (drones[i].estado == OPERATIVO) {
            drones[i].apt = aptitud(drones[i].vis, drones[i].dist, drones[i].bat);
        }
    }   

    Dron orden[N_DRONES + 1];
    for (int i = 0; i <= N_DRONES; i++) orden[i] = drones[i];
    size_t n = N_DRONES;
    qsort(orden + 1, n, sizeof(orden[0]), comparar);

    printf("Orden por aptitud:\n");
    for (size_t i = 1; i <= n; i++) {
        if (drones[i].estado == OPERATIVO) {
            printf("C%d (%.2f)\n", drones[i].id, drones[i].apt);
        }
    }
    printf("\n");

    int lider = eleccion(1);
    printf("Nuevo líder: C%d\n", lider);
    return 0;
}