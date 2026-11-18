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
#define PORT 8080

char ip[] = "127.0.0.1";
typedef struct Faro {
    int id;
    double bat;
    double alc;
    double lat;
    int estado;
    double aptitud;
} Faro;

Faro faros[N + 1] = {
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
    return apt;
}

int cmp(const void *a, const void *b){
    int x = *(const int *)a; /* id del faro a*/
    int y = *(const int *)b; /* id del faro b*/
    if(faros[x].aptitud < faros[y].aptitud) return 1;
    if(faros[x].aptitud > faros[y].aptitud) return -1;
    return 0;
}

void enviar(int destino, const char *msg){

}

int recibir(char *buf, int timeout_ms){

}

int main(void){
    int order[N + 1];
    for (int i = 1; i <= N; i++){
        order[i] = i;
        if(faros[i].estado == CAIDO) continue;
        faros[i].aptitud = aptitud(&faros[i]);
    }

    qsort(order + 1, N, sizeof(int), cmp);

    for (int i = 1; i <= N; i++){
        printf("F%d apt: %.4f\n", order[i], faros[order[i]].aptitud);
    }

    // Por cada faro 
    for (int i = 1; i <= N; i++){
        // Si es que está vivo
        if(faros[i].estado == OPERATIVO){

            // Creo un hijo
            pid_t pid = fork();

            if(pid < 0){ perror("fork"); return 1;}

            // Proceso hijo
            if(pid == 0){
                // Socket
                int client_fd = socket(AF_INET, SOCK_STREAM, 0);
                if(client_fd < 0){
                    perror("socket");
                    return 1;
                }

                struct sockaddr addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(PORT)
                };
            }

            // Proceso padre
            if(pid > 0){

            }

        }
    }
}