# Guía de Ejercicios — Elección de Líder (formato Control 03)

**Sistemas Distribuidos — Preparación Certamen 2**

> Todos los ejercicios tienen **el mismo formato que el Control 03 (CENTINELA)**:
> un escenario con telemetría, un líder que cae, y dos partes:
> **1. Función de aptitud** y **2. Protocolo de elección en C**.
>
> **Solo código.** Las justificaciones (pesos, por qué gana quien gana) van como **comentarios** dentro del `.c`, tal como en la prueba.
>
> **Reglas para todos los ejercicios:**
> - Un archivo `.c` por ejercicio, que compile con `gcc -Wall -Wextra` **sin warnings**. Sin pseudocódigo.
> - El programa debe **imprimir** el ranking de aptitud y **todos los mensajes**, desde la detección de la caída hasta que el último nodo reconoce al nuevo líder.
> - Formato de mensaje: `[C1 -> C5] ELECTION`.
> - **Tiempo objetivo: 30 min por ejercicio.**
>
> | Ej | Escenario | Algoritmo | Lo nuevo respecto del control |
> |---|---|---|---|
> | 1 | CENTINELA (el control) | Bully | nada: el control tal cual, de memoria |
> | 2 | CENTINELA | Anillo | otro algoritmo |
> | 3 | CENTINELA | Raft | otro algoritmo + mayoría |
> | 4 | Servidores | Bully clásico | criterio inverso, normalización, `struct`/`enum` |
> | 5 | Boyas | Anillo | 2 criterios inversos + heartbeat por ciclos |
> | 6 | Servidores | Bully y Anillo | un nodo cae **durante** la elección |
> | 7 | Servidores | Raft | dos candidatos a la vez (votos divididos) |

---

## Ejercicio 1 — El control, de memoria (`ej1_centinela_bully.c`)

Resuelve el [Control 03](control3.md) completo **desde cero, sin mirar `centinela.c`**, con cronómetro.

1. **Aptitud:** tu `F(batería, distancia, visibilidad)` con la justificación de los pesos en un comentario, el cálculo para C1, C2, C4, C5 y C6, y el ranking impreso de mayor a menor.
2. **Elección:** Bully, desde que C1 detecta la caída de C3 hasta que todos reconocen al nuevo líder.

**Tu salida debe mostrar:**
- [ ] El ranking con el valor de F de cada dron.
- [ ] El ELECTION de C1 hacia los drones más aptos que él.
- [ ] El OK de cada dron vivo que lo recibe. C3 no responde.
- [ ] El COORDINATOR del ganador a **todos** los vivos.
- [ ] Una línea final `Nuevo líder: Cx`, que coincide con el primero de tu ranking.

**Si te pasaste de 30 min**, anota en qué parte se te fue el tiempo y repítelo otro día.

---

## Ejercicio 2 — El control con Anillo (`ej2_centinela_anillo.c`)

Mismo escenario y misma `F` del Ej. 1, pero con **Anillo**.

- Orden del anillo: C1 → C2 → C3 → C4 → C5 → C6 → C1.
- Inicia **C1**. El mensaje ELECTION lleva al **mejor candidato visto hasta ahora** (id y aptitud).
- Cuando el mensaje vuelve a C1, este da la vuelta de COORDINATOR.

**Tu salida debe mostrar:**
- [ ] `[C2 -> C3] ELECTION (sin respuesta)` y el salto a C4, **en las dos vueltas**.
- [ ] El candidato que lleva el mensaje en cada salto: `[C4 -> C5] ELECTION(cand=C1)`.
- [ ] La vuelta de COORDINATOR completa, hasta volver a C1.
- [ ] El mismo líder que en el Ej. 1.

---

## Ejercicio 3 — El control con Raft (`ej3_centinela_raft.c`)

Mismo escenario y misma `F`, pero con **Raft**.

- 6 drones en total, así que la mayoría es `6/2 + 1 = 4`. C3 cuenta en el total aunque esté caído.
- Usa la aptitud en el **election timeout**: el más apto despierta primero.
- Regla de voto: un voto por term, y solo si la aptitud del candidato es mayor o igual que la propia.
- El nuevo líder se da a conocer con `HEARTBEAT(term)` a todos.

**Implementa con arrays** `term[]` y `voto[]`, e imprime ambos antes y después de la votación.

**Tu salida debe mostrar:**
- [ ] Quién se postula y con qué term.
- [ ] Cada `REQUEST_VOTE` y cada `VOTE`. C3 no vota.
- [ ] `votos >= MAYORIA` y los HEARTBEAT del nuevo líder.
- [ ] El mismo líder que en los Ej. 1 y 2.

**Variación (5 min):** fuerza a que el **menos apto** despierte primero. Tu programa debe mostrar que no llega a la mayoría, y luego hacer una segunda elección en el **term 2** que gane el más apto.

---

## Ejercicio 4 — Servidores: Bully clásico con structs (`ej4_servidores_bully.c`)

### Briefing
Un clúster de **5 servidores** (N1–N5) replica una base de datos. El líder **N5** deja de responder. Lo detecta **N4**.

| Nodo | CPU libre (%) | Latencia (ms) | Uptime (h) | Estado |
|------|---------------|---------------|------------|--------|
| N1   | 70            | 40            | 120        | Activo |
| N2   | 55            | 15            | 300        | Activo |
| N3   | 90            | 80            | 50         | Activo |
| N4   | 40            | 25            | 500        | Activo |
| N5   | 80            | 30            | 200        | **Caído** (era el líder) |

### 1. Aptitud (función dada)
Normaliza con min–max, calculando los mínimos y máximos **recorriendo el arreglo**. La latencia es un criterio **inverso**:

```
norm(x) = (x - min) / (max - min)
F = 0.5 · norm(cpu) + 0.3 · (1 - norm(latencia)) + 0.2 · norm(uptime)
```

Ordena con `qsort`. La normalización usa los 5 nodos, N5 incluido, porque la telemetría es previa a la caída.

### 2. Elección: Bully clásico
Esta vez **sin `break`**: el iniciador manda ELECTION a **todos** los más aptos, y **cada** nodo que responde OK inicia su propia elección (una sola vez por nodo).

**Representación obligatoria:**
```c
typedef enum { ACTIVO, CAIDO } Estado;
typedef enum { ELECTION, OK, COORDINATOR } TipoMsg;
typedef struct { int id; double cpu, lat, uptime, aptitud; Estado estado; } Nodo;
typedef struct { TipoMsg tipo; int origen, destino; } Mensaje;

int enviar(Nodo red[], Mensaje m);   /* imprime y devuelve 1 si el destino está ACTIVO */
```

### Resultado esperado
```
N5  0.6974
N2  0.5611
N1  0.5157
N3  0.5000
N4  0.4538

...
Nuevo líder: N2
ELECTION: 10   OK: 6   COORDINATOR: 3   Total: 19
```
(Los 10 ELECTION incluyen los 4 que se le mandan a N5, que no responde.)

---

## Ejercicio 5 — Boyas: dos criterios inversos + heartbeat (`ej5_boyas_anillo.c`)

### Briefing
Una red de **6 boyas oceanográficas** (B1–B6) tiene una boya coordinadora, que reúne los datos y los transmite por satélite a la costa. La coordinadora es **B5**. Envía heartbeat cada **10 s**, y si una boya no lo recibe en **3 ciclos seguidos**, la declara caída e inicia una elección.

| Boya | Energía solar (%) | Dist. a la costa (km) | Pérdida de paquetes (%) | Señal satelital (0–100) |
|------|-------------------|-----------------------|-------------------------|-------------------------|
| B1   | 85                | 12.0                  | 2.0                     | 60                      |
| B2   | 40                | 3.5                   | 0.5                     | 90                      |
| B3   | 70                | 8.0                   | 5.0                     | 75                      |
| B4   | 95                | 20.0                  | 1.0                     | 40                      |
| B5   | —                 | —                     | —                       | —  (coordinadora, cae)  |
| B6   | 75                | 15.0                  | 8.0                     | 70                      |

### 1. Aptitud
Diseña tu `F`: **distancia** y **pérdida de paquetes** son inversos. Los pesos deben sumar 1, con la justificación de cada uno en un comentario. Imprime el ranking de B1, B2, B3, B4 y B6.

### 2. Detección + elección (Anillo)
**Detección:** la boya **B1** registra este arreglo de heartbeats (dado):
```c
int hb[] = {1, 1, 0, 1, 0, 0, 0};   /* 1 = llegó el HB de B5 en ese ciclo */
```
- El ciclo c ocurre a las `08:00:00 + 10·c`.
- Lleva un contador de fallos seguidos que se reinicia al llegar un HB.
- Al llegar a 3 fallos: `══ B5 INOPERATIVA — INICIAR ELECCIÓN ══` y se deja de recorrer.

**Elección:** Anillo B1 → B2 → B3 → B4 → B5 → B6 → B1, iniciado por B1. El mensaje lleva la **lista de todos los que lo vieron**:
```c
typedef struct { TipoMsg tipo; int lista[6]; int n; } MsgAnillo;
```
Al volver a B1, este elige al más apto de la lista y da la vuelta de COORDINATOR.

### Salida esperada (detección)
```
[08:00:00] HB recibido
[08:00:10] HB recibido
[08:00:20] HB de B5: TIMEOUT (fallo 1/3)
[08:00:30] HB recibido
[08:00:40] HB de B5: TIMEOUT (fallo 1/3)
[08:00:50] HB de B5: TIMEOUT (fallo 2/3)
[08:01:00] HB de B5: TIMEOUT (fallo 3/3)
[08:01:00] ══ B5 INOPERATIVA — INICIAR ELECCIÓN ══
```
La elección debe mostrar la lista creciendo, `{1}`, `{1,2}`, ..., `{1,2,3,4,6}`, y saltar a B5 en ambas vueltas. El ganador debe ser el primero de tu ranking.

---

## Ejercicio 6 — Un nodo cae durante la elección (`ej6_falla_eleccion.c`)

Mismo escenario y misma `F` del Ej. 4. Parte copiando tu código del Ej. 4 y agrega una variable global que diga **qué nodo cae y en qué momento**.

### Caso A — Bully: N2 manda sus OK y cae antes de enviar COORDINATOR
- Los nodos que recibieron OK de N2 esperan el COORDINATOR. Como no llega, imprimen `TIMEOUT esperando COORDINATOR` y **reinician** la elección.
- **Resultado esperado:** `Nuevo líder: N1`.
- En un comentario: por qué cambia el ganador y por qué la elección igual concluye.

### Caso B — Anillo (N1 → N2 → N3 → N4 → N5 → N1, inicia N4)
- **B1:** N3 cae justo después de reenviar el ELECTION. La lista que vuelve a N4 es `{4, 1, 2, 3}`.
  - **Resultado esperado:** `Nuevo líder: N2`, igual que sin falla.
- **B2:** N2 cae justo después de reenviar el ELECTION. La lista que vuelve es la misma, pero el más apto ya no existe.
  - Antes de anunciar, N4 le manda un `PING` al elegido. Si no responde, lo quita de la lista y elige al siguiente más apto.
  - **Resultado esperado:** `[N4 -> N2] PING (sin respuesta)` y luego `Nuevo líder: N1`.

---

## Ejercicio 7 — Raft: dos candidatos a la vez (`ej7_raft_split.c`)

Mismo escenario y misma `F` del Ej. 4: 5 nodos, mayoría 3, N5 caído.

**N1 y N3** agotan su timeout **al mismo tiempo** y se postulan ambos en el **term 1**. Los REQUEST_VOTE llegan en este orden (dado):

```c
/* {candidato, destino}, en orden de llegada */
int llegadas[][2] = {
    {1, 4}, {3, 2}, {1, 2}, {3, 4}, {1, 3}, {3, 1}, {1, 5}, {3, 5}
};
```

### Parte A — regla normal (un voto por term, al primero que lo pide)
- Procesa las llegadas en orden e imprime la tabla `term[]`/`voto[]` al final del term 1.
- Si nadie llegó a la mayoría, haz el **term 2**: N1 despierta primero y se postula solo.
- **Resultado esperado:** term 1 con N1 = 2 votos y N3 = 2 votos, **sin líder**. Term 2 con N1 = 4 votos, **N1 líder**.

### Parte B — con regla de aptitud (solo voto si `apt[cand] >= apt[yo]`)
- Repite el term 1 con la misma lista de llegadas.
- **Resultado esperado:** N1 = 2 votos, N3 = 1 voto, sin líder.
- En un comentario: ¿puede ganar alguien que no sea el más apto con esta regla? Da un ejemplo con estos datos.
