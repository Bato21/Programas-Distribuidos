#include <stdio.h>

#define N 6

int    vivo[N + 1] = {0, 1, 1, 0, 1, 1, 1};   /* C3 caído */
double apt[N + 1];                              /* aptitud de cada dron */

int eleccion(int yo) {
    printf("C%d inicia elección\n", yo);
    int respondio = 0;

    /* 1. ELECTION a todos los más aptos que yo */
    for (int i = 1; i <= N; i++) {
        if (i != yo && apt[i] > apt[yo]) {
            printf("  C%d -> C%d : ELECTION\n", yo, i);
            if (vivo[i]) {                      /* si está vivo, responde */
                printf("  C%d -> C%d : OK\n", i, yo);
                respondio = i;
                break;  /* no necesito preguntar a los demás */
            }
        }
    }

    /* 2. Nadie más apto respondió => gano yo */
    if (respondio == 0) {
        for (int i = 1; i <= N; i++)
            if (i != yo && vivo[i])
                printf("  C%d -> C%d : COORDINATOR\n", yo, i);
        return yo;
    }

    /* 3. Alguien más apto respondió: él sigue la elección */
    return eleccion(respondio);
}

float aptitud(int bateria, float distancia, float visibilidad) {
    if(distancia <= 0.5){
        return bateria/2.0 + 100 + visibilidad;
    }
    else if(distancia < 1.5){
        return bateria/2.0 + 50 + visibilidad;
    }
    else {
        return bateria/2.0 + visibilidad + 20;
    }
}

int main(void) {
    /* aquí llenas apt[] con tu F(batería, distancia, visibilidad) */
    /* Favorecemos primero visibilidad, luego distancia y finalmente bateria
    Esto por que no sirve de nada tener a un líder lejos de la bóveda y que además no ve nada.
    Este mismo protocolo funciona en caso de que la batería de uno
    de los drones se agote, si es que ya no tiene batería dejaría de responder y se 
    provocarían automáticamente nuevas elecciones para elegir al nuevo líder más apto.
    Sin embargo esto no significa que la batería restante es insignificante, la tomamos en cuenta
    en el calculo de la aptitud pero solo con un coeficiente menor.*/
    
    apt[1] = aptitud(78, 1.2, 85);
    apt[2] = aptitud(45, 0.8, 92);
    apt[4] = aptitud(91, 2.1, 70);
    apt[5] = aptitud(62, 0.5, 88);
    apt[6] = aptitud(83, 1.5, 45);
    int lider = eleccion(1);                   /* C1 detectó la caída de C3 */
    printf("Nuevo líder: C%d\n", lider);
    return 0;
}