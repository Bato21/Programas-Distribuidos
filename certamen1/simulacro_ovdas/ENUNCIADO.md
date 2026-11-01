# Simulacro Certamen — Red de Vigilancia Volcánica OVDAS

**Taller de Ciencia de Redes — Sistemas Distribuidos y Comunicación entre Procesos**
**Puntaje:** 100 puntos  **Tiempo:** 60 minutos

> Simulacro hecho a partir del Control 01 (protocolo + checksum + servidor con `fork`),
> del Ejercicio 3 del certamen pasado (programa con pipes internos que además es cliente TCP)
> y de la P2 de la guía (pipeline de 3 procesos con 2 pipes y un filtro).

**Instrucciones**
- El código debe estar en C, con todos los `#include` necesarios.
- Implemente manejo de errores apropiado (`perror`, verificación de retornos).
- La función `checksum()` y el struct `Umbral` están dados: **no los modifique**.
- Cada proceso debe cerrar **inmediatamente después del `fork()`** los extremos de pipe que no usa.
- Lea el enunciado completo antes de escribir código.

---

## Contexto

Chile tiene más de 90 volcanes activos. El **Observatorio Volcanológico de los Andes del Sur (OVDAS)**
vigila los más peligrosos con estaciones sismológicas instaladas en sus faldeos. Cada estación funciona
de forma autónoma: un sismógrafo registra la actividad del suelo de forma continua, pero buena parte de
esas lecturas no sirve. Hay **ruido ambiental** (viento, tráfico, animales) que produce lecturas muy bajas,
y a veces el instrumento **falla** y entrega valores físicamente imposibles (negativos). Enviar todo eso
por el enlace satelital sería caro e inútil, así que cada estación aplica un **filtro local** y solo transmite
las lecturas válidas a la **Central OVDAS** en Temuco.

La Central recibe datos de varias estaciones a la vez, verifica que no se hayan corrompido en el enlace
(checksum), y evalúa cada lectura con el **semáforo de alerta volcánica** de SERNAGEOMIN:
VERDE, AMARILLA, NARANJA o ROJA. Al terminar el turno, cada estación pide un resumen de su sesión.

## Arquitectura

```
          ESTACIÓN (un solo programa: estacion.c, 3 procesos)                    CENTRAL (central.c)
 ┌───────────────────────────────────────────────────────────────────┐
 │ SISMÓGRAFO (hijo 1) ──pipe1──► FILTRO (hijo 2) ──pipe2──► TRANSMISOR (padre) ──TCP 7000──► [ CENTRAL OVDAS ]
 └───────────────────────────────────────────────────────────────────┘                          (fork por estación)
   ./estacion VILLARRICA  ──┐
   ./estacion LLAIMA      ──┼──────────────────────────────────────────────────────────►  ./central
   ./estacion CALBUCO     ──┘
```

- **Pipes**: comunicación LOCAL entre procesos del mismo programa (dentro de la estación).
- **Sockets TCP**: comunicación por RED entre cada estación y la Central.

## Protocolo

Texto plano, cada mensaje termina en `\n`.

**Dentro de la estación (pipes):**

| Canal | Mensaje |
|---|---|
| pipe1: Sismógrafo → Filtro | `LECT <seq> <magnitud>\n` |
| pipe2: Filtro → Transmisor | `VALIDA <seq> <magnitud>\n` |

**Estación ↔ Central (socket TCP, puerto 7000):**

| Dirección | Mensaje |
|---|---|
| Estación → Central | `CONNECT <volcan>\n` |
| Central → Estación | `HELLO <volcan>\n` o `REJECT <volcan>\n` |
| Estación → Central | `SISMO <volcan> <seq> <magnitud> <cs>\n` |
| Central → Estación | `ACK <volcan> <seq> <nivel>\n` o `NACK <volcan> <seq>\n` |
| Estación → Central | `FIN <volcan>\n` |
| Central → Estación | `RESUMEN <volcan> <recibidas> <alertas>\n` |

- `<magnitud>` se envía siempre con 2 decimales (`"%.2f"`), y `<cs>` es `checksum()` de ese texto.
- `<nivel>` es `VERDE`, `AMARILLA`, `NARANJA` o `ROJA`.
- `<recibidas>` = lecturas con ACK; `<alertas>` = lecturas con nivel distinto de VERDE.

**Umbrales por volcán** (magnitud local):

| Volcán | AMARILLA ≥ | NARANJA ≥ | ROJA ≥ |
|---|---|---|---|
| VILLARRICA | 2.0 | 3.5 | 5.0 |
| LLAIMA | 2.5 | 4.0 | 5.5 |
| CALBUCO | 2.0 | 3.0 | 4.5 |
| LASCAR | 3.0 | 4.5 | 6.0 |

Por debajo del umbral AMARILLA el nivel es VERDE.

---

## Parte A — Central OVDAS (`central.c`) — 40 pts

Complete `handle_estacion()` y `main()` en el esqueleto.

**TODO A.1 (10 pts) — Conexión inicial.**
Recibir `CONNECT <volcan>`, buscar el volcán en la tabla `umbrales[]` y responder `HELLO` o `REJECT`.
Si es REJECT, la atención de esa estación termina.

**TODO A.2 (18 pts) — Loop de lecturas.**
- Recibir `SISMO`, validar que el volcán sea el de esta conexión y que el checksum calce. Si no: `NACK`.
- Si es válida: implementar `nivel_alerta()` (no viene en el esqueleto), responder `ACK <volcan> <seq> <nivel>`
  y llevar los contadores `recibidas` y `alertas`.
- Imprimir en la Central: `[CENTRAL] VILLARRICA seq=3 M=3.71 -> NARANJA`
- **Ojo:** las `seq` pueden llegar con saltos (el filtro descarta lecturas). Eso NO es un error.
- Si recibe `FIN <volcan>`: responder `RESUMEN` y terminar la atención.
- Si la estación se desconecta (`recv` devuelve 0), terminar sin caerse.

**TODO A.3 (12 pts) — Servidor concurrente.**
Servidor TCP en el puerto 7000 que atiende **varias estaciones a la vez** con `fork()`, sin dejar procesos zombi.

---

## Parte B — Estación (`estacion.c`) — 50 pts

Se ejecuta como `./estacion <VOLCAN>` (la IP de la Central es `127.0.0.1`).

**TODO B.1 (12 pts) — Procesos y pipes.**
Crear `pipe1` y `pipe2`, y luego 2 hijos: **hijo 1 = Sismógrafo**, **hijo 2 = Filtro**. El **padre es el Transmisor**.
Cada proceso cierra los extremos que no usa justo después del `fork()`, y cada hijo termina con `exit()`.

**TODO B.2 (8 pts) — Sismógrafo (hijo 1).**
Genera **12 lecturas** con `seq` de 1 a 12. Cada magnitud es aleatoria entre **-1.0 y 7.0**.
Escribe `LECT <seq> <magnitud>\n` en `pipe1`, imprime `[SISMOGRAFO] LECT 4 3.71` y hace `sleep(1)` entre lecturas.

**TODO B.3 (10 pts) — Filtro (hijo 2).**
Lee de `pipe1` hasta que el Sismógrafo cierre el pipe. Por cada lectura:
- `magnitud < 0.0` → `[FILTRO] Descartada seq=5 (falla de sensor: -0.42)`
- `0.0 ≤ magnitud < 1.0` → `[FILTRO] Descartada seq=6 (ruido: 0.73)`
- `magnitud ≥ 1.0` → la reenvía por `pipe2` como `VALIDA <seq> <magnitud>\n`

**TODO B.4 (20 pts) — Transmisor (padre, cliente TCP).**
1. Conectarse a la Central, enviar `CONNECT` y verificar el `HELLO`. Si recibe `REJECT` o falla la conexión,
   cerrar `pipe2[0]` (los hijos terminarán solos) y esperar a los hijos antes de salir.
2. Leer de `pipe2` hasta que el Filtro cierre el pipe. Por cada `VALIDA`: armar el texto de la magnitud con `"%.2f"`,
   calcular el checksum, enviar `SISMO`, recibir la respuesta e imprimirla: `[TRANSMISOR] ACK VILLARRICA 4 NARANJA`
3. Al terminar: enviar `FIN`, recibir e imprimir el `RESUMEN`, cerrar el socket y hacer `wait()` de **los dos** hijos.

---

## Parte C — Preguntas cortas — 10 pts

1. **(3 pts)** ¿Por qué el Sismógrafo y el Filtro se comunican por pipe, pero el Transmisor y la Central por socket?
   ¿Se podría usar un pipe entre la estación y la Central? Justifique.
2. **(4 pts)** Si el **Filtro** olvida hacer `close(pipe1[1])` después del `fork()`, ¿qué le pasa al programa
   cuando el Sismógrafo termina sus 12 lecturas? Explique por qué.
3. **(3 pts)** En la Central, ¿por qué el proceso hijo cierra `server_fd` y el padre cierra `client_fd`?
   ¿Qué pasa si el padre nunca llama a `waitpid()`?

---

## Ejemplo de ejecución

**Terminal 1**
```
$ ./central
[CENTRAL] Escuchando en puerto 7000...
[CENTRAL] VILLARRICA conectada
[CENTRAL] VILLARRICA seq=1 M=2.18 -> AMARILLA
[CENTRAL] VILLARRICA seq=3 M=1.40 -> VERDE
[CENTRAL] VILLARRICA seq=4 M=5.62 -> ROJA
...
[CENTRAL] VILLARRICA FIN: recibidas=8 alertas=5
[CENTRAL] Rechazada estación desconocida: OSORNO
```

**Terminal 2**
```
$ ./estacion VILLARRICA
[TRANSMISOR] Conectado a la Central como VILLARRICA
[SISMOGRAFO] LECT 1 2.18
[TRANSMISOR] ACK VILLARRICA 1 AMARILLA
[SISMOGRAFO] LECT 2 -0.42
[FILTRO] Descartada seq=2 (falla de sensor: -0.42)
[SISMOGRAFO] LECT 3 1.40
[TRANSMISOR] ACK VILLARRICA 3 VERDE
[SISMOGRAFO] LECT 4 5.62
[TRANSMISOR] ACK VILLARRICA 4 ROJA
...
[TRANSMISOR] RESUMEN VILLARRICA 8 5
[TRANSMISOR] Estación finalizada
```

**Terminal 3**
```
$ ./estacion OSORNO
[TRANSMISOR] La Central rechazó la conexión: REJECT OSORNO
```

Compilación:
```
gcc -Wall -o central central.c
gcc -Wall -o estacion estacion.c
```

---

## Referencia rápida

| Función | Uso |
|---|---|
| `pipe(int fd[2])` | `fd[0]` lectura, `fd[1]` escritura. -1 si falla |
| `fork()` | 0 en el hijo, PID del hijo en el padre, -1 si falla |
| `read(fd, buf, n)` / `write(fd, buf, n)` | 0 en `read` = el otro extremo se cerró (EOF) |
| `wait(NULL)` / `waitpid(-1, NULL, WNOHANG)` | esperar hijos / recoger zombis sin bloquear |
| `socket`, `bind`, `listen`, `accept` | servidor TCP |
| `socket`, `inet_pton`, `connect` | cliente TCP |
| `send` / `recv` | `recv` = 0 → el otro lado cerró la conexión |
| `snprintf`, `sscanf`, `strcmp`, `strcspn`, `atof` | armar y leer mensajes |
| `srand(getpid())`, `(float)rand() / RAND_MAX` | número aleatorio entre 0.0 y 1.0 |
