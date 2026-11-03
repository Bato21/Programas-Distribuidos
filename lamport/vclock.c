/* vclock.c — relojes vectoriales */
#include <stdio.h>
#include <string.h>

#define NPROC 3

typedef struct {
    int v[NPROC];  /* v[i] = conocimiento sobre Pi */
    int pid;
} VNode;

void vc_local(VNode *n) {
    /* TODO: incrementa solo la propia entrada */
    n->v[n->pid]++; 
}

void vc_send(VNode *n, int out[NPROC]) {
    /* TODO: incrementa, copia vector a out */
    vc_local(n);
    for (int i = 0; i < NPROC; i++){
        out[i] = n->v[i];
    }
}

void vc_recv(VNode *n, const int rcv[NPROC]) {
    /* TODO: máximo componente a componente, luego incrementa */
    for (int i=0; i < NPROC; i++){
        // El componente recibido es mayor, reemplazamos por el recibido y sumamos 1
        if (n->v[i] < rcv[i]) n->v[i] = rcv[i];
        // Si es menor o son iguales solo sumamos 1
        else continue;
    }
    vc_local(n);
}

/* a → b  sii  ∀i: a[i] ≤ b[i]  y  ∃j: a[j] < b[j] */
int vc_happens_before(const int a[NPROC], const int b[NPROC]) {
    /* TODO */
    int es_mayor = 0;
    int es_igual = 0;

    for (int i = 0; i < NPROC; i++){
        if (a[i] < b[i]) continue;
        else if (a[i] == b[i]) es_igual++;
        else es_mayor++;
    }

    // Si en ningun caso a > b y no pasa que a = b en todos los casos, entonces a → b y retornamos 1
    if (es_mayor == 0 && es_igual < NPROC) return 1;
    // Si no retornamos 0 ya que no se puede determinar
    else return 0;
}

int vc_concurrent(const int a[NPROC], const int b[NPROC]) {
    /* TODO: usa vc_happens_before */
    // Si vc_happens_before devuelve 0, entonces hay procedencia y la concurrencia devuelve 0
    if (vc_happens_before(a, b) == 1 || vc_happens_before(b, a) == 1) return 0;
    // Si no hay procedencia, hay concurrencia y retornamos 1
    else return 1;
}

void print_vc(const char *label, const int v[NPROC]) {
    printf("%s: [%d,%d,%d]\n", label, v[0], v[1], v[2]);
}

int main(void) {
    VNode p0 = {{0,0,0}, 0};
    VNode p1 = {{0,0,0}, 1};
    VNode p2 = {{0,0,0}, 2};
    int ts[NPROC], snap_a[NPROC], snap_b[NPROC];

    vc_local(&p0);                       /* evento a */
    memcpy(snap_a, p0.v, sizeof(ts));
    print_vc("P0 tras a", p0.v);

    vc_local(&p2);                       /* evento b — independiente */
    memcpy(snap_b, p2.v, sizeof(ts));
    print_vc("P2 tras b", p2.v);

    vc_send(&p0, ts);                    /* P0 envía m1 */
    vc_recv(&p1, ts);                    /* P1 recibe m1 */
    print_vc("P1 tras recv m1", p1.v);

    printf("a → P1_actual: %s\n",
           vc_happens_before(snap_a, p1.v) ? "SÍ" : "NO");
    printf("a ∥ b:         %s\n",
           vc_concurrent(snap_a, snap_b) ? "SÍ" : "NO");
    return 0;
}
