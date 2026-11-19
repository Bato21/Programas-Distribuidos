#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>   /* struct timeval para SO_RCVTIMEO */

#define N 5
#define MAYORIA (N / 2 + 1)   /* 5/2 + 1 = 3 votos. Se cuenta el TOTAL de faros, F3 incluido */
#define OPERATIVO 1
#define CAIDO 0 
#define MAX_BAT 92
#define MIN_BAT 55
#define MAX_ALC 20
#define MIN_ALC 8
#define MAX_LAT 120
#define MIN_LAT 40
#define PORT 8080
#define BUF_SIZE 256

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

int sock;

void enviar_msg(int destino, const char *msg){
    char buf[BUF_SIZE] = {0};
    snprintf(buf, sizeof(buf), "%d %s", destino, msg);
    send(sock, buf, sizeof(buf), 0);    
}

int recibir_msg(char *buf, int timeout_ms){
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int n = recv(sock, buf, BUF_SIZE, 0);
    if (n > 0) return n;
    else return 0;
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
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if(sock < 0){
                    perror("socket");
                    return 1;
                }

                struct sockaddr_in addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(PORT)
                };

                inet_pton(AF_INET, ip, &addr.sin_addr);

                if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
                    perror("connect");
                    return 1;
                }

                char enviar[BUF_SIZE];
                // Mandamos id
                snprintf(enviar, sizeof(enviar), "CONNECT F%d\n", i);
                send(sock, enviar, sizeof(enviar), 0);
                /* =====================================================
                 * PARTE 3: ELECCIÓN RAFT
                 * ===================================================== */

                /* --- Paso 1: election timeout según la posición en el ranking ---
                 * order[] se ordenó ANTES del fork, así que cada hijo lo heredó.
                 * Busco en qué posición k quedó mi id:
                 *   F2 → k=1 → 200 ms   (el más apto despierta primero)
                 *   F5 → k=2 → 300 ms
                 *   F1 → k=3 → 400 ms
                 *   F4 → k=4 → 500 ms
                 * Así el más apto es el primero en notar que no hay líder. */
                int k = 1;
                while (order[k] != i) k++;
                int timeout = 200 + 100 * (k - 1);

                /* --- Paso 2: estado Raft de ESTE faro ---
                 * Cada faro es un proceso distinto, así que no hace falta un arreglo
                 * term[N+1]: cada uno guarda solo SU term y SU voto.
                 *   term = en qué elección (época) cree estar este faro
                 *   voto = a quién votó en ese term (0 = todavía a nadie)  */
                int term = 0, voto = 0;
                char buf[BUF_SIZE], msg[BUF_SIZE];

                while (1) {
                    /* --- Paso 3: esperar un mensaje, como máximo "timeout" ms ---
                     * Si llega algo, lo proceso más abajo y vuelvo a esperar con el
                     * timeout completo (cada mensaje "reinicia el reloj").
                     * Si NO llega nada: no hay líder vivo → me postulo. */
                    if (!recibir_msg(buf, timeout)) {

                        /* --- Paso 4: me hago CANDIDATO ---
                         * term++  : abro una elección nueva; los votos de terms viejos no sirven.
                         * voto = i: voto por mí mismo. Gasto mi único voto de este term,
                         *           así no se lo puedo dar a otro candidato del mismo term
                         *           → como máximo UN líder por term. */
                        term++;
                        voto = i;
                        int votos = 1;
                        printf("[F%d] %d ms sin HEARTBEAT -> CANDIDATO term %d\n", i, timeout, term);

                        /* Pido el voto a TODOS, F3 incluido: no sé quién está vivo.
                         * Si el destino está caído, la estación costera lo descarta. */
                        for (int j = 1; j <= N; j++) {
                            if (j == i) continue;
                            sprintf(msg, "REQUEST_VOTE %d %d", term, i);
                            enviar_msg(j, msg);
                        }

                        /* --- Paso 5: juntar votos durante 100 ms ---
                         * recibir_msg devuelve 0 cuando pasan 100 ms sin que llegue nada:
                         * ahí doy por terminada la votación. */
                        while (recibir_msg(buf, 100)) {
                            int t, origen;
                            /* sscanf devuelve cuántos campos calzaron: 2 = era un VOTE.
                             * t == term: ignoro votos de elecciones viejas. */
                            if (sscanf(buf, "VOTE %d %d", &t, &origen) == 2 && t == term) {
                                votos++;
                                printf("[F%d] VOTE de F%d (%d/%d)\n", i, origen, votos, MAYORIA);
                            }
                        }

                        /* --- Paso 6: ¿llegué a la mayoría? ---
                         * Mayoría sobre el TOTAL (3 de 5), no sobre los vivos.
                         * Si no llego, vuelvo al while(1): si otra vez pasa el timeout
                         * sin noticias, me postulo de nuevo en un term nuevo. */
                        if (votos < MAYORIA) {
                            printf("[F%d] %d votos, sin mayoría\n", i, votos);
                            continue;
                        }

                        /* --- Paso 7: soy LÍDER → HEARTBEAT a todos ---
                         * En Raft no hay mensaje COORDINATOR: el líder se da a conocer
                         * mandando heartbeats con su term. */
                        printf("[F%d] LIDER del term %d con %d votos\n", i, term, votos);
                        for (int j = 1; j <= N; j++) {
                            if (j == i) continue;
                            sprintf(msg, "HEARTBEAT %d %d", term, i);
                            enviar_msg(j, msg);
                        }
                        /* (Parte 4 iría aquí: APPEND / ACK / COMMIT.)
                         * Espero un poco antes de cerrar para que la estación costera
                         * alcance a reenviar los HEARTBEAT. */
                        sleep(1);
                        break;
                    }

                    /* --- Paso 8: llegó un mensaje → lo proceso según su tipo --- */
                    int t, origen;

                    if (sscanf(buf, "REQUEST_VOTE %d %d", &t, &origen) == 2) {
                        /* Regla 1: si el candidato está en un term MAYOR que el mío,
                         * me paso a ese term y quedo "sin votar" en él.
                         * (Se hace SIEMPRE, vote o no vote: así nadie reabre un term viejo.) */
                        if (t > term) {
                            term = t;
                            voto = 0;
                        }
                        /* Regla 2: un voto por term. Solo voto si es el term actual
                         * y todavía no voté en él. */
                        if (t == term && voto == 0) {
                            voto = origen;
                            printf("[F%d] voto por F%d en el term %d\n", i, origen, t);
                            sprintf(msg, "VOTE %d %d", t, i);
                            enviar_msg(origen, msg);   /* el VOTE va solo al candidato */
                        }
                    }
                    else if (sscanf(buf, "HEARTBEAT %d %d", &t, &origen) == 2) {
                        /* El HEARTBEAT es lo que hace que la red RECONOZCA al nuevo líder. */
                        term = t;
                        printf("[F%d] reconozco a F%d como lider (term %d)\n", i, origen, t);
                        /* (Con la parte 4, aquí NO se termina: se sigue esperando el APPEND.) */
                        break;
                    }
                }

                close(sock);
                exit(0);
            }

            // Proceso padre
            if(pid > 0){

            }

        }
    }

    while(wait(NULL) > 0);
    exit(0);
}