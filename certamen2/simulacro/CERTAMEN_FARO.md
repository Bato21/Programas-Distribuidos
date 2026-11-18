# Simulacro Certamen 2 — Protocolo FARO

**Tiempo:** 90 min
**Puntaje total:** 60 puntos
**Archivo:** `faro.c`, que compile con `gcc -Wall -Wextra -o faro faro.c` **sin warnings**. Sin pseudocódigo.

---

### Briefing de misión

La Armada mantiene una red de **5 faros automáticos** (F1–F5) en el Estrecho de Magallanes. Funcionan bajo un modelo de **líder único**: el faro líder coordina el modo de operación de toda la red (normal, niebla, tormenta) y lo replica al resto, para que todos los faros tengan **el mismo registro de comandos**.

La red usa **Raft**:
- El líder envía `HEARTBEAT` periódicamente.
- Si un faro no recibe nada durante su **election timeout**, se postula como candidato en un term nuevo.
- Un líder necesita la **mayoría del total de faros** (no solo de los vivos).
- Los comandos del líder se agregan a un **log replicado**, y una entrada queda **confirmada (commit)** cuando la tiene la mayoría.

Cada faro corre como un **proceso independiente** y se comunica con los demás **solo por sockets UDP** en `127.0.0.1`.

---

### Registro de incidente — 03:15

El faro **F3**, líder de la red, sufrió una falla eléctrica a las 03:15:00. Desde ese momento no envía heartbeats.

#### Telemetría de la red al momento del incidente

| Faro | Batería (%) | Alcance de radio (km) | Latencia (ms) | Estado |
|---|---|---|---|---|
| F1 | 80 | 12 | 90 | Operativo |
| F2 | 55 | 20 | 40 | Operativo |
| F3 | — | — | — | **Caído** (era el líder) |
| F4 | 92 | 8 | 120 | Operativo |
| F5 | 70 | 15 | 50 | Operativo |

En la latencia, **un menor valor indica mayor aptitud**.

---

### Su misión

#### 1. Función de aptitud (15 puntos)

- **a)** Defina `F(batería, alcance, latencia)` como función en C. Ponga la fórmula y la justificación de los pesos **en un comentario** encima, y explique cómo trata el criterio inverso.
- **b)** El programa debe imprimir la aptitud de los 4 faros operativos, ordenados de mayor a menor (`qsort` o búsqueda del máximo).

#### 2. Comunicación por sockets (15 puntos)

- **a)** El `main` calcula las aptitudes y luego crea **un proceso hijo por cada faro operativo** con `fork()`. F3 no se crea, porque está caído.
- **b)** Cada hijo abre un socket UDP y hace `bind` al puerto `6000 + id` (F1 → 6000+1 = 6001, ..., F5 → 6005).
- **c)** Implemente dos funciones:
  ```c
  void enviar(int destino, const char *msg);   /* sendto al puerto 6000 + destino */
  int  recibir(char *buf, int timeout_ms);     /* 1 si llegó algo, 0 si venció el timeout */
  ```
  Para el timeout use `setsockopt(... SO_RCVTIMEO ...)`. Si `recvfrom` devuelve `-1`, venció el tiempo.
- **d)** Los mensajes son **texto** con este formato (`term` = term del emisor, `id` = id del emisor):

  | Mensaje | Formato | Quién lo envía |
  |---|---|---|
  | Pedir voto | `REQUEST_VOTE <term> <id>` | candidato → todos |
  | Dar voto | `VOTE <term> <id>` | votante → candidato |
  | Latido | `HEARTBEAT <term> <id>` | líder → todos |
  | Replicar | `APPEND <term> <indice> <comando>` | líder → todos |
  | Confirmar recepción | `ACK <term> <id>` | seguidor → líder |
  | Confirmar commit | `COMMIT <term> <indice>` | líder → todos |

  Para leerlos use `sscanf` y para armarlos `sprintf`.

#### 3. Elección Raft (20 puntos)

- **a)** Election timeout de cada faro: `200 + 100 · (posición en el ranking − 1)` ms. El más apto espera 200 ms, el segundo 300 ms, y así sucesivamente.
- **b)** Si `recibir()` vence, el faro hace `term++`, vota por sí mismo y envía `REQUEST_VOTE` a **todos** los demás faros (F3 incluido: el UDP se pierde y nadie contesta).
- **c)** Regla de voto: un faro vota **una sola vez por term**. Si recibe un term mayor al suyo, se actualiza a ese term antes de decidir.
- **d)** El candidato junta los `VOTE` durante 100 ms. Si llega a `MAYORIA = N/2 + 1` (3 de 5), envía `HEARTBEAT` a todos. Cada seguidor que lo recibe imprime que reconoce al nuevo líder.
- **e)** En un comentario: qué faro resulta líder y por qué.

#### 4. Replicación del log (10 puntos)

- **a)** Cada faro tiene un log local: un arreglo de strings y un contador de entradas.
- **b)** Una vez elegido, el líder agrega `SET modo=niebla` a su log y envía `APPEND` a todos. Cada seguidor lo agrega a su log, lo imprime y responde `ACK`.
- **c)** El líder cuenta los ACK (más su propia copia). Si llega a la mayoría, imprime `COMMIT` y envía `COMMIT` a todos. Al recibirlo, cada seguidor termina con `exit(0)`, y el líder termina tras enviarlo.
- **d)** Además de la pantalla, cada faro escribe sus eventos en su archivo `faro<id>.log`, con `fopen` / `fprintf`.

---

### Variante B (después, 20 min extra)

Agregue un argumento: `./faro 2` significa que **el faro 2 se cae justo después de ganar la elección, antes de enviar su primer HEARTBEAT** (hace `exit(0)`).

Su programa debe mostrar, **sin cambiar nada más**:
1. Que los seguidores no reciben heartbeat y vence el timeout del siguiente del ranking.
2. Una nueva elección en el **term 2**, ganada con exactamente la mayoría.
3. La entrada `SET modo=niebla` confirmada por el nuevo líder.

En un comentario: ¿qué pasaría si además cayera un segundo faro? ¿Por qué Raft no elige líder en ese caso, aunque Bully sí lo haría?

---

### Ayuda: llamadas de sockets UDP

```c
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int s = socket(AF_INET, SOCK_DGRAM, 0);

struct sockaddr_in dir = {0};
dir.sin_family      = AF_INET;
dir.sin_port        = htons(6000 + id);
dir.sin_addr.s_addr = inet_addr("127.0.0.1");

bind(s, (struct sockaddr *)&dir, sizeof dir);                          /* para recibir */
sendto(s, msg, strlen(msg), 0, (struct sockaddr *)&dir, sizeof dir);   /* dir = la del destino */
int n = recvfrom(s, buf, TAM - 1, 0, NULL, NULL);                      /* -1 si vence el timeout */

struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
```

**Ojo:**
- Haga `fflush(stdout)` antes de los `fork()`. Si no, cada hijo hereda el buffer y vuelve a imprimir el ranking.
- `recvfrom` no termina el string: ponga `buf[n] = '\0'`.
- El `main` debe esperar a los hijos con `while (wait(NULL) > 0);`.

---

### Autoevaluación

- [ ] ¿Compila sin warnings?
- [ ] ¿La fórmula, los pesos y el criterio inverso están justificados en un comentario?
- [ ] ¿Se imprime el ranking **una sola vez**, con los valores de F?
- [ ] ¿Cada faro es un proceso distinto y **toda** la comunicación pasa por `sendto` / `recvfrom`?
- [ ] ¿Se postula primero el más apto de tu ranking, en el term 1?
- [ ] ¿El líder gana con al menos 3 votos y los 3 seguidores imprimen que lo reconocen?
- [ ] ¿Los 3 seguidores tienen `log[1] = SET modo=niebla` y el líder imprime `COMMIT`?
- [ ] ¿Existen `faro1.log`, `faro2.log`, `faro4.log` y `faro5.log` con los eventos de cada uno (y **no** `faro3.log`)?
- [ ] ¿El programa **termina solo**, sin quedarse colgado?
- [ ] Variante B: ¿el segundo del ranking gana el **term 2** con exactamente 3 votos?

**Referencia:** con pesos iguales y min–max sobre los 4 operativos, el ranking es F2 (2.000), F5 (1.864), F1 (1.384), F4 (1.000). Gana F2 en el term 1; en la Variante B, F5 en el term 2. Si usaste otros pesos, el líder debe ser **el primero de tu ranking**.

Cuando lo termines, pásame tu `faro.c` y lo revisamos.
