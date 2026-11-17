# Preparación Certamen 2 — Elección de Líder

1. **Plan:** en qué orden hacer la [GUIA.md](GUIA.md).
2. **Checklist de código:** lo que tienes que poder escribir sin mirar.
3. **Simulacro VIGÍA:** mismo formato que el Control 03. Hazlo con cronómetro (60 min) al terminar la guía.

---

## 1. Plan

| Orden | Qué | Tiempo | Listo cuando... |
|---|---|---|---|
| 1 | Ej 1: el control con Bully, de memoria | 30 min | lo terminas a tiempo sin mirar `centinela.c` |
| 2 | Ej 2 y 3: el control con Anillo y con Raft | 1 h | sale el mismo líder con los tres algoritmos |
| 3 | Ej 4: servidores, con structs y criterio inverso | 30 min | tu salida calza con el resultado esperado (N2, 19 mensajes) |
| 4 | Ej 5: boyas, con heartbeat + Anillo | 30 min | tu log calza con el esperado |
| 5 | Ej 6 y 7: falla durante la elección, Raft dividido | 1 h | calzan los resultados esperados |
| 6 | Simulacro VIGÍA | 60 min | lo terminas a tiempo |
| 7 | Ej 1 otra vez | 30 min | lo haces en **20 min** |

Si te falta tiempo: **Ej 1 → Simulacro → Ej 1 otra vez**. El certamen va a ser como el control.

---

## 2. Checklist de código

**Esqueleto que se repite en todos**
- [ ] `#define N`, arreglos `vivo[N + 1]` y `apt[N + 1]` (índice 0 sin usar), o un arreglo de `struct Nodo`.
- [ ] `typedef enum` para los tipos de mensaje y `typedef struct` para nodo y mensaje.
- [ ] Una función que imprima un mensaje: `printf("[C%d -> C%d] %s\n", origen, destino, nombre)`.
- [ ] `printf("Nuevo líder: C%d\n", lider);` al final.

**Aptitud**
- [ ] `double aptitud(...)`, con los pesos justificados en un comentario encima.
- [ ] Criterio inverso: `1 - norm(x)` o restar con signo negativo, explicado en el comentario.
- [ ] División con decimales (`2.0`, no `2`).
- [ ] Ranking: `qsort` con un comparador de mayor a menor, o un ciclo que busque el máximo.

**Algoritmos**
- [ ] **Bully:** ELECTION a los más aptos → OK de los vivos → recursión → COORDINATOR a todos.
- [ ] **Anillo:** `siguiente_vivo()` con `i % N + 1` → vuelta ELECTION → vuelta COORDINATOR.
- [ ] **Raft:** `term[]`, `voto[]`, `MAYORIA = N/2 + 1` → `term++` + voto propio → REQUEST_VOTE/VOTE → HEARTBEAT.

**Detección**
- [ ] Recorrer `hb[]` con un contador de fallos que se reinicia al llegar un HB → al llegar a 3, imprimir `INOPERATIVO` → llamar a la elección.

---

## 3. Simulacro — Protocolo VIGÍA

**Tiempo:** 60 min.
**Puntaje total:** 60 puntos

---

### Briefing de misión

El Observatorio Vulcanológico del Sur vigila el volcán Lonquimay con una red de **7 estaciones sísmicas** (E1–E7) repartidas por sus laderas. Las estaciones operan de forma autónoma bajo un modelo de líder único: la estación **concentradora** reúne las lecturas de todas, coordina los turnos de muestreo y retransmite los datos a la ciudad por radioenlace.

La concentradora emite un heartbeat cada **4 segundos**. El protocolo VIGÍA establece que si una estación no recibe heartbeat durante **3 ciclos consecutivos**, debe declarar a la concentradora como inoperativa e iniciar un proceso de elección de nueva concentradora.

La aptitud de cada estación para asumir el rol se evalúa según tres criterios operacionales:

- **Batería disponible (%)**
- **Temperatura interna (°C)**: los equipos se degradan con el calor.
- **Latencia del radioenlace (ms)**

---

### Registro de incidente — Turno nocturno

La estación **E6**, concentradora de la red, dejó de responder a las 22:40:00. El log de la estación **E2** registra:

```
[22:40:04] HB de E6: TIMEOUT (fallo 1/3)
[22:40:08] HB de E6: TIMEOUT (fallo 2/3)
[22:40:12] HB de E6: TIMEOUT (fallo 3/3)
[22:40:12] ══ PROTOCOLO VIGÍA: E6 INOPERATIVA ══
[22:40:12] ══ INICIAR ELECCIÓN DE NUEVA CONCENTRADORA ══
```

#### Telemetría de la red al momento del incidente

| Estación | Batería (%) | Temp. interna (°C) | Latencia (ms) | Estado |
|---|---|---|---|---|
| E1 | 88 | 41 | 120 | Operativa |
| E2 | 52 | 28 | 60 | Operativa |
| E3 | 75 | 35 | 45 | Operativa |
| E4 | 95 | 55 | 90 | Operativa |
| E5 | 64 | 30 | 150 | Operativa |
| E6 | — | — | — | **Inoperativa** |
| E7 | 70 | 33 | 70 | Operativa |

*Todas las estaciones operativas se comunican entre sí. Cada estación conoce la composición completa de la red y puede enviar mensajes directos a cualquier otra estación operativa.*

---

### Su misión

#### 1. Función de aptitud (20 puntos)

Defina una función de aptitud `F(batería, temperatura, latencia)` que asigne un valor numérico a cada estación, representando su idoneidad para asumir el rol de concentradora.

- **a)** Presente la fórmula de su función de aptitud y justifique el peso o tratamiento que asigna a cada criterio, considerando que en dos de ellos **un menor valor indica mayor aptitud**.
- **b)** Calcule el valor de aptitud de cada estación operativa (E1, E2, E3, E4, E5, E7) y ordénelas de mayor a menor aptitud.

#### 2. Protocolo de elección (40 puntos)

Seleccione un algoritmo de consenso o elección de líder estudiado en el curso e impleméntelo para este escenario. Debe indicar explícitamente cuál algoritmo selecciona.

- **a)** Implemente el protocolo de elección en C. El código debe modelar el intercambio de mensajes entre las estaciones, desde que E2 detecta la caída de E6 hasta que la nueva concentradora es reconocida por toda la red. **No se acepta pseudocódigo.**
- **b)** Determine qué estación resulta elegida como nueva concentradora según su implementación y justifique el resultado.

---

### Variante B (después, 20 min extra)

Toma tu solución y agrégale, en el mismo `.c`, lo que el profe dijo que podía aparecer:

1. **Detección:** reemplaza el log fijo por un arreglo `int hb[] = {1, 0, 1, 0, 0, 0};`. El ciclo c ocurre a las `22:39:52 + 4·c`. Tu programa debe generar exactamente el log del registro (ojo con el HB perdido del ciclo 1, que no es una caída).
2. **Falla durante la elección:** la estación que **iba a ganar** pierde la conexión justo después de participar en la elección y antes de ser anunciada. Tu programa debe detectarlo con un mensaje sin respuesta, seguir con las estaciones restantes e imprimir la nueva concentradora.

---

### Autoevaluación

- [ ] ¿Compila sin warnings con `gcc -Wall -Wextra`?
- [ ] ¿La fórmula y la justificación de los pesos están como comentario?
- [ ] ¿Temperatura y latencia se tratan como inversos, y explicaste cómo?
- [ ] ¿El programa imprime el valor de F de las 6 estaciones, ordenado?
- [ ] ¿Escribiste explícitamente qué algoritmo elegiste?
- [ ] ¿La salida parte con la detección de E2 y termina con un mensaje que reciben **todas** las estaciones operativas?
- [ ] ¿E6 aparece sin responder cuando le llega un mensaje?
- [ ] ¿La estación elegida coincide con el primero de tu ranking, y lo justificaste en un comentario?
- [ ] ¿Lo terminaste en 60 minutos?

Cuando lo termines, pásame tu `vigia.c` y lo revisamos.
