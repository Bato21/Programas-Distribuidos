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
#define N 6
#define MAYORIA (N * 0.5 + 1)

// En que elecciones vamos
int term[N + 1];
// Por quien voto i en el term
int voto[N + 1];

void aptitud(Dron *dron){
    float apt = 0;
    apt += dron->vis;
    apt += (dron->bat * 0.5);
    if (dron->dist <= 0.5) apt += 100;
    else if (dron->dist < 1.5) apt += 50;
    else apt += 20;
    dron->apt = apt;
    return;
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

// Timeout de cada dron
int timeout_ms(int i){ return 300 + (int)drones[i].apt; }

int comparar(Dron drona, Dron dronb){
    if(drona.apt > dronb.apt) return 1;
    else if (dronb.apt > drona.apt) return -1;
    else return 0;
}


int contar_vivos(Dron drones[]){
    int vivos = 0;
    for (int i = 0; i <= N; i++){
        if(drones[i].estado == OPERATIVO) vivos++;
    }
    return vivos;
}

int hay_quorum(Dron drones[]){
    int vivos = contar_vivos(drones);
    if (vivos >= (N * 0.5) + 1) return 1;
    else return 0;
}

/* Imprime term[] y voto[] como tabla, para ver la evolución */
void imprimir_estado(const char *titulo) {
    printf("\n   %s\n", titulo);
    printf("            ");
    for (int i = 1; i <= N; i++) printf("  C%d", i);
    printf("\n   term[] = ");
    for (int i = 1; i <= N; i++) printf(drones[i].estado ? "%4d" : "   x", term[i]);
    printf("\n   voto[] = ");
    for (int i = 1; i <= N; i++) {
        if (!drones[i].estado)          printf("   x");
        else if (voto[i] == 0) printf("   -");
        else                   printf("  C%d", voto[i]);
    }
    printf("\n\n");
}

int election(int id){
    printf("\nSe ha iniciado una nueva elección\n\n");
    imprimir_estado("Nueva elección");
    // Sin quorum no hay elecciones
    if (hay_quorum(drones) == 0){
        printf("No hay quorum para las elecciones\n");
        return 1;
    }
    // Si hay, buscamos al candidato que se postula
    printf("Quorum alcanzado\n");
    int cand = id;
    printf("C%d: pasaron %d ms sin HB -> me hago CANDIDATO\n", cand, timeout_ms(cand));
    // nueva eleccion
    term[cand]++;
    // candidato vota por si mismo
    voto[cand] = cand;
    // votos por el candidato
    int votos = 1;

    imprimir_estado("Nueva votación");
    // se hace request de voto por cada dron
    for (int i = 1; i <= N; i++){
        // Si es el candidato se skipea
        if(i == cand) continue;
        // Hago la solicitud
        printf("C%d -> C%d REQUEST VOTE (term %d)\n", cand, i, term[cand]);
        // Si no está operativo marcamos que no responde y skipeamos
        if(drones[i].estado == INOPERATIVO){ printf("(C%d no responde)\n", i); continue;}

        // Si mi term es más chico actualizo el term 
        if(term[i] < term[cand]){
            term[i] = term[cand];
            if (drones[i].apt <= drones[cand].apt){
                voto[i] = cand;
                votos++;
                printf("C%d -> C%d VOTE (votos%d)\n", i, cand, votos);
            }
        }
    }

    imprimir_estado("Fin proceso votación");

    // Revisamos si llegamos a la mayoría
    if(votos < MAYORIA){
        printf("Mayoría no alcanzada, %d votos de %f\n", votos, MAYORIA);
        printf("\nEsperando nueva elección...\n\n");
        return 0;
    }
    // Si hay votos mandamos HB al resto de los drones
    for (int i = 1; i <= N; i++){
        if(i == cand || drones[i].estado == INOPERATIVO) continue;
        printf("C%d -> C%d HEARTBEAT term[%d]\n", cand, i, term[cand]);
    }

    return cand;

}

int main(void) {
    // Rellenamos las aptitudes
    for (int i = 1; i <= N; i++){
        if (drones[i].estado == OPERATIVO) {
            //printf("Dron C%d vivo\n", i);
            aptitud(&drones[i]);
            //printf("Aptitud dron C%d=%f\n",i, drones[i].apt);
        }
    }  
    int lider = election(6);          /* C6 despierta primero */
    if (lider == 0)                   /* no llegó a la mayoría */
        lider = election(5);          /* luego despierta C5 */
    printf("Nuevo líder: C%d\n", lider);
    return 0;
}