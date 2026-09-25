#include <stdio.h>
#include <pthread.h>

void *saludar(void *arg) {
   long id = (long)arg;
   printf("Hilo %ld\n", id);
   return NULL;
}

int main() {
   pthread_t h[4];
   for (long i=0; i<4; i++)
      pthread_create(&h[i],NULL,saludar,(void*)i);
   for (int i=0; i<4; i++)
      pthread_join(h[i],NULL);
   return 0;
}
