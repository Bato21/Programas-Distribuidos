#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/socket.h>

#define N 5
#define OPERATIVO 1
#define CAIDO 0 
#define MAX_BAT 92
#define MIN_BAT 55
#define MAX_ALC 20
#define MIN_ALC 8
#define MAX_LAT 120
#define MIN_LAT 40


typedef struct Faro {
    int id;
    double bat;
    double alc;
    double lat;
    int estado;
    double aptitud;
} Faro;

Faro faros = {
    {0, 0, 0, 0, 0, 0},
    {1, 80, 12, 90, OPERATIVO, 0},
    {2, 55, 20, 40, OPERATIVO, 0},
    {3, 0, 0, 0, CAIDO, 0},
    {4, 92, 8, 120, OPERATIVO, 0},
    {5, 70, 15, 50, OPERATIVO, 0}
};

double normalizar(double x, double max, double min){
    return ((x - min) / (max - min));
}

double aptitud(const Faro *faro){
    double apt = 0;
    // Sumamos la batería normalizada a su aptitud
    apt += normalizar(faro->bat, MAX_BAT, MIN_BAT);
    // Sumamos el alcance normalizado a su aptitud
    apt += normalizar(faro->alc, MAX_ALC, MIN_ALC);
    // Sumamos 1 - la latencia normalizada a su aptitud
    // Mientras menor la latencia mejor -> relacion inversa
    // Por eso 1 - ese valor
    apt += (1 - normalizar(faro->lat, MAX_LAT, MIN_LAT));
}