#include <stdio.h>
#include <pthread.h>

#define NHILOS 4
#define ITER   100000

long contador = 0;
pthread_mutex_t lock;
int usar_mutex = 0;

void *sumar(void *arg) {
    for (int i = 0; i < ITER; i++) {
        if (usar_mutex) pthread_mutex_lock(&lock);
        contador++;
        if (usar_mutex) pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    usar_mutex = (argc > 1);            // ./contador m  -> con mutex
    pthread_t h[NHILOS];
    pthread_mutex_init(&lock, NULL);

    for (int i = 0; i < NHILOS; i++)
        pthread_create(&h[i], NULL, sumar, NULL);
    for (int i = 0; i < NHILOS; i++)
        pthread_join(h[i], NULL);

    pthread_mutex_destroy(&lock);
    printf("%s: %ld (esperado %d)\n", usar_mutex ? "con mutex" : "sin mutex",
           contador, NHILOS * ITER);
    return 0;
}
