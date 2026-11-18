#include <stdio.h>
#include <stdlib.h>

#define N 7
#define OPERATIVA 1
#define INOPERTATIVA 0
#define MAX_BAT 95
#define MIN_BAT 52
#define MAX_TEMP 55
#define MIN_TEMP 28
#define MAX_LAT 150
#define MIN_LAT 45

typedef struct Estacion {
    int id;
    double bat;
    double tmp;
    double lat;
    int estado;
    double aptitud;
} Estacion;

Estacion estaciones[N + 1] = {
    {0, 0, 0, 0, INOPERTATIVA, 0},
    {1, 88, 41, 120, OPERATIVA, 0},
    {2, 52, 28, 60, OPERATIVA, 0},
    {3, 75, 35, 45, OPERATIVA, 0},
    {4, 95, 55, 90, OPERATIVA, 0},
    {5, 64, 30, 150, OPERATIVA, 0},
    {6, 0, 0, 0, INOPERTATIVA, 0},
    {7, 70, 33, 70, OPERATIVA, 0}
};

double normalizar(double x, double max, double min){
    return ((x - min) / (max - min));
}

double aptitud(const Estacion *estacion){
    double resultado = 0;
    resultado += normalizar(estacion->bat, MAX_BAT, MIN_BAT);
    resultado += 1 - normalizar(estacion->lat, MAX_LAT, MIN_LAT);
    resultado += 1 - normalizar(estacion->tmp, MAX_TEMP, MIN_TEMP);
    return resultado;
}

int cmp(const void *a, const void *b) {
    int x = *(const int *)a;          /* id del nodo a */
    int y = *(const int *)b;          /* id del nodo b */
    if (estaciones[x].aptitud < estaciones[y].aptitud) return 1;   /* mayor primero */
    if (estaciones[x].aptitud > estaciones[y].aptitud) return -1;
    return 0;
}

int eleccion (int yo){
    int cand = yo;
    int respondio = 0;
    for (int i = 1; i <= N; i++){
        if(i != yo){
            printf("E%d -> E%d ELECTION\n", yo, i);
            if (estaciones[i].estado != OPERATIVA){
                printf("(E%d no respondió)\n", i);
            }
            // Si le pregunto a alguien más fuerte que yo
            if(estaciones[i].aptitud > estaciones[yo].aptitud && estaciones[i].estado == OPERATIVA){
                // Me responde
                printf("E%d -> E%d OK\n", i, yo);
                respondio = i;
                cand = eleccion(respondio);
                break;
            }
        }
    }

    // Si nadie respondio le aviso al resto que soy lider
    if (respondio == 0) {
        for (int i = 1; i <= N; i++){
            if (i != cand){
                if(estaciones[i].estado == OPERATIVA)
                    printf("E%d -> E%d COORDINATOR\n", cand, i);
                else printf("C%d no responde\n", i);
            }
        }
    }

    return cand;
}

int main(void){
    
    int orden[N + 1];

    for (int i = 1; i <= N; i++){
        if(estaciones[i].estado == OPERATIVA){
            // Llenamos las aptitudes
            estaciones[i].aptitud = aptitud(&estaciones[i]);
        }
        // Llenamos la copia de las estaciones para ordenar
        orden[i] = i;
        
    }

    qsort(orden + 1, N, sizeof(int), cmp);
    for (int i = 1; i <= N; i ++) printf("%d.- E%d\n", i, orden[i]);

    int lider = eleccion(7);

    printf("Nuevo lider: E%d\n", lider);

}