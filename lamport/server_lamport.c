#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 7070

typedef struct {
    int  ts;
    char payload[64];
} Msg;

static int global_clock = 0;

void handle_client(int fd) {
    Msg msg, resp;
    while (recv(fd, &msg, sizeof(msg), 0) > 0) {

        /* 1. Actualizar reloj al recibir */
        if (global_clock < msg.ts) {
            global_clock = msg.ts;
        }
        global_clock++;
        printf("[srv clock=%d] recibido '%s'\n", global_clock, msg.payload);

        /* 2. Evento interno de procesamiento */
        global_clock++;

        /* 3. Preparar respuesta con timestamp */
        snprintf(resp.payload, sizeof(resp.payload), "ACK");
        resp.ts = global_clock;

        send(fd, &resp, sizeof(resp), 0);
    }
}

/*
 * 2. Si se usa fork() para manejar clientes, global_clock no se comparte
 * automáticamente. Cada proceso hijo recibe una copia del espacio de memoria
 * del padre, por lo que las modificaciones realizadas por un hijo no son
 * visibles para los demás procesos. En consecuencia, no existe un único reloj
 * global coherente entre todos los clientes.
 *
 * Para mantener un reloj compartido habría que utilizar memoria compartida,
 * por ejemplo mmap() o shm_open(), y proteger el acceso con un semáforo POSIX
 * u otro mecanismo de sincronización entre procesos. Otra alternativa es que
 * cada proceso mantenga su propio reloj Lamport, aceptando que no habrá un
 * reloj único para todo el servidor.
 *
 * 3. Con pthread, todos los hilos comparten el mismo espacio de memoria y,
 * por lo tanto, acceden al mismo global_clock. Si dos hilos actualizan el
 * reloj simultáneamente puede ocurrir una condición de carrera y perderse
 * alguna actualización.
 *
 * Para resolverlo se debe proteger con un pthread_mutex_t la operación
 * completa de lectura, comparación y actualización:
 *
 *     pthread_mutex_lock(&clock_mutex);
 *     global_clock = max(global_clock, msg.ts) + 1;
 *     pthread_mutex_unlock(&clock_mutex);
 *
 * También se deben proteger el incremento del evento interno y la asignación
 * del timestamp de respuesta para mantener consistente la secuencia completa.
 */