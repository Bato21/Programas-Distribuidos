/*
 * Raft — elección de líder (versión simple para estudiar)
 *
 * Compilar:  gcc -Wall -Wextra -o raft raft.c
 * Ejecutar:  ./raft
 *
 * El programa imprime los arrays en cada paso para ver cómo cambian.
 */
#include <stdio.h>

#define N       6
#define MAYORIA (N / 2 + 1)     /* 6/2 + 1 = 4 votos. Se cuenta el TOTAL (C3 incluido aunque esté caído) */

/*
 * Estado inicial del enjambre (índice 0 no se usa):
 *
 *            idx:   0     1      2      3     4      5      6
 *   vivo[]      = { -,    1,     1,     0,    1,     1,     1   }   C3 caído
 *   apt[]       = { -,  174,  164.5,    0, 135.5,  219,  106.5 }   tu F()
 *   term[]      = { -,    0,     0,     0,    0,     0,     0   }   todos en la "época" 0
 *   voto[]      = { -,    0,     0,     0,    0,     0,     0   }   0 = no ha votado en su term
 */
int    vivo[N + 1] = {0, 1, 1, 0, 1, 1, 1};
double apt[N + 1]  = {0, 174, 164.5, 0, 135.5, 219, 106.5};
int    term[N + 1];        /* term[i] = en qué term (época) cree estar el dron i     */
int    voto[N + 1];        /* voto[i] = a quién votó i en ese term (0 = a nadie aún)  */

/* Imprime term[] y voto[] como tabla, para ver la evolución */
void imprimir_estado(const char *titulo) {
    printf("\n   %s\n", titulo);
    printf("            ");
    for (int i = 1; i <= N; i++) printf("  C%d", i);
    printf("\n   term[] = ");
    for (int i = 1; i <= N; i++) printf(vivo[i] ? "%4d" : "   x", term[i]);
    printf("\n   voto[] = ");
    for (int i = 1; i <= N; i++) {
        if (!vivo[i])          printf("   x");
        else if (voto[i] == 0) printf("   -");
        else                   printf("  C%d", voto[i]);
    }
    printf("\n\n");
}

/*
 * Election timeout: cuánto aguanta cada dron sin HB antes de postularse.
 * En Raft real es ALEATORIO (150–300 ms). Aquí lo hacemos depender de la aptitud
 * para que el más apto "despierte" primero:
 *
 *   C1: 300 - 174 = 126 ms
 *   C2: 300 - 164 = 136 ms
 *   C4: 300 - 135 = 165 ms
 *   C5: 300 - 219 =  81 ms   <- el primero en despertar
 *   C6: 300 - 106 = 194 ms
 */
int timeout_ms(int i) { return 300 - (int)apt[i]; }

int eleccion_raft(void) {
    imprimir_estado("Estado inicial (C3 acaba de caer):");

    /* ---------------------------------------------------------------
     * PASO 1: ¿quién se postula?
     * El que tenga el timeout más corto => C5 (81 ms).
     * --------------------------------------------------------------- */
    int cand = 0;
    for (int i = 1; i <= N; i++)
        if (vivo[i] && (cand == 0 || timeout_ms(i) < timeout_ms(cand)))
            cand = i;
    printf("C%d: pasaron %d ms sin HB -> me hago CANDIDATO\n", cand, timeout_ms(cand));

    /* ---------------------------------------------------------------
     * PASO 2: el candidato abre un term nuevo y vota por sí mismo.
     *
     *   ¿Por qué term++?  Es una elección NUEVA. Los votos de terms
     *   anteriores ya no sirven: cada dron tiene 1 voto POR TERM.
     *
     *   ¿Por qué vota por él mismo?
     *     a) Él también es parte del enjambre y su voto cuenta para la
     *        mayoría (4 de 6). Si no votara, necesitaría 4 votos AJENOS.
     *     b) Al votar por sí mismo GASTA su único voto de este term.
     *        Si en este mismo term otro dron también se postulara, C5
     *        ya no le puede dar su voto => no pueden ganar los dos.
     *        Eso garantiza: como mucho UN líder por term.
     *
     *             C1  C2  C3  C4  C5  C6
     *   term[] =   0   0   x   0   1   0     <- solo C5 está en el term 1
     *   voto[] =   -   -   x   -  C5   -     <- C5 votó por C5
     * --------------------------------------------------------------- */
    term[cand]++;
    voto[cand] = cand;
    int votos = 1;
    imprimir_estado("PASO 2: C5 abre el term 1 y vota por sí mismo (1 voto):");

    /* ---------------------------------------------------------------
     * PASO 3: REQUEST_VOTE(term 1) a todos los demás.
     *
     * Regla de cada dron i al recibirlo:
     *   si term del candidato > mi term  => es un term nuevo para mí,
     *                                       aún no he votado en él => VOTO
     *   si no                            => ya voté en este term => no voto
     *
     * Ej. C1 recibe REQUEST_VOTE(term 1): su term es 0, 1 > 0 => vota.
     *     Queda term[1] = 1, voto[1] = C5.
     * Ej. C3 está caído: nunca contesta.
     * --------------------------------------------------------------- */
    for (int i = 1; i <= N; i++) {
        if (i == cand) continue;
        printf("  C%d -> C%d : REQUEST_VOTE(term %d)\n", cand, i, term[cand]);

        if (!vivo[i]) {
            printf("        (C%d no responde)\n", i);
            continue;
        }
        if (term[cand] > term[i]) {
            term[i] = term[cand];   /* me paso al term nuevo        */
            if (apt[i] > apt[cand]) {
                printf("  C%d: candidato menos apto que yo -> NO VOTO\n", i);
            } else {
                voto[i] = cand;         /* gasto mi voto en el candidato */
                votos++;
                printf("  C%d -> C%d : VOTE   (votos: %d)\n", i, cand, votos);
            }
        }
    }
    /*
     * Al terminar el paso 3:
     *             C1  C2  C3  C4  C5  C6
     *   term[] =   1   1   x   1   1   1
     *   voto[] =  C5  C5   x  C5  C5  C5     => 5 votos
     */
    imprimir_estado("PASO 3: después de pedir los votos:");

    /* ---------------------------------------------------------------
     * PASO 4: ¿llegó a la mayoría?  5 >= 4 => SÍ.
     * No hay mensaje COORDINATOR: el nuevo líder empieza a mandar
     * HEARTBEAT(term 1) y con eso todos lo reconocen.
     * --------------------------------------------------------------- */
    printf("C%d: %d votos de %d (mayoría = %d)\n", cand, votos, N, MAYORIA);
    if (votos < MAYORIA) {
        printf("C%d: sin mayoría, habrá otra elección en un term nuevo\n", cand);
        return 0;
    }
    for (int i = 1; i <= N; i++)
        if (i != cand && vivo[i])
            printf("  C%d -> C%d : HEARTBEAT(term %d)\n", cand, i, term[cand]);
    return cand;
}

int main(void) {
    int lider = eleccion_raft();
    printf("\nNuevo líder: C%d\n", lider);
    return 0;
}
