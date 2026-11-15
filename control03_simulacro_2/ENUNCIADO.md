# Simulacro Control 03 — Red de Drones de Rescate CÓNDOR

**Taller de Ciencia de Redes — Sistemas Distribuidos**
**Puntaje:** 100 puntos  **Tiempo:** 60 minutos

> Simulacro con el formato del Control 01 (servidor + cliente con TODOs), acumulativo:
> sockets, `fork`, pipes, `select`, **relojes de Lamport**, **log replicado con número de secuencia**,
> **heartbeats** y **quórum**. Al final hay preguntas de desarrollo (sin puntaje) para repasar la teoría.

**Instrucciones**
- El código debe estar en C. Los `#include`, las constantes, los `struct` y las funciones marcadas como **DADO** ya están en los esqueletos ([base.c](base.c) y [dron.c](dron.c)): **no los modifiques**.
- Implementa manejo de errores básico (`perror`, revisar retornos).
- Cada proceso cierra, **justo después del `fork()`**, los descriptores que no usa.
- Lee el enunciado completo antes de escribir código.
- Puedes consultar la tabla de funciones del final.

---

## Contexto

Tras un terremoto, el servicio de emergencias despliega una flota de hasta **5 drones** de búsqueda. Cada dron vuela de forma autónoma y reporta a la **Base CÓNDOR** dos tipos de mensajes:

- **Heartbeats** (`HB`) cada 1 segundo: "sigo vivo".
- **Eventos** (`EVT`) numerados: despegue, hallazgos, pedidos de rescate…

La Base mantiene un **log de eventos por dron**, que debe quedar **completo y en orden**: si se pierde el evento #3, la Base no puede aplicar el #4 y le pide al dron que reenvíe desde el #3.

Como los drones vuelan sobre una zona sin infraestructura, la Base **solo toma decisiones si tiene quórum**: necesita que la **mayoría** de la flota (3 de 5) esté viva. Sin quórum, deja los eventos en espera.

Todos los mensajes llevan un **timestamp de Lamport**, para que el log tenga un orden causal consistente.

```
[DRON-1] ──┐
[DRON-2] ──┤
[DRON-3] ──┼── TCP 7300 ──►  [ BASE CÓNDOR ]
[DRON-4] ──┤                  padre: select + reloj + log + heartbeats
[DRON-5] ──┘                  un hijo "cartero" por dron ──pipe──► padre
```

## Protocolo

Texto plano. Cada mensaje termina en `\n`. **`<T>` siempre es el reloj de Lamport del que envía**, ya incrementado según la regla de envío.

| Dirección | Mensaje | Significado |
|---|---|---|
| Dron → Base | `JOIN <id> <T>` | pide unirse a la flota |
| Base → Dron | `WELCOME <id> <T>` | aceptado |
| Base → Dron | `REJECT <id> <T>` | id fuera de 1..5, o ese id ya está conectado |
| Dron → Base | `HB <id> <T>` | heartbeat, cada 1 s |
| Dron → Base | `EVT <id> <seq> <T> <texto>` | evento número `seq` (el texto puede tener espacios) |
| Base → Dron | `ACK <seq> <T>` | evento `seq` aplicado al log (o ya estaba) |
| Base → Dron | `NACK <seq> <T>` | no se aplicó: reenvía **desde** el evento `seq` |

### Reglas de Lamport (las mismas en la Base y en el dron)
- **Enviar**: `reloj = reloj + 1` y el mensaje sale con ese valor.
- **Recibir** un mensaje con `T`: `reloj = max(reloj, T) + 1`.

---

## Parte A — Base CÓNDOR (`base.c`)  (60 pts, ~35 min)

**La arquitectura es obligatoria:**
- El **padre** es el único que tiene el **reloj de Lamport**, el arreglo `drones[]` y el log. También es el único que **responde** a los drones (por eso guarda el socket de cada uno en `drones[id].fd`).
- Cada **hijo** solo lee el socket de su dron y le pasa los mensajes al padre por **una pipe compartida**, como `Aviso`.
- El padre espera con **`select()`** sobre el socket servidor y la pipe, con **timeout de 1 segundo**.

### TODO A.1 — Inicialización y nuevos drones (20 pts)
1. Crear el socket servidor en el puerto `PORT` (`socket`, `setsockopt` con `SO_REUSEADDR`, `bind`, `listen`) y crear la pipe.
2. Bucle principal: `select()` sobre el socket servidor y el extremo de lectura de la pipe, con timeout de 1 s.
3. Si hay un dron nuevo:
   - `accept`, recibir `JOIN <id> <T>` y aplicar la **regla de recepción**.
   - Si el formato es inválido, el id no está en 1..5 o `drones[id].estado != LIBRE` → **regla de envío**, responder `REJECT` y cerrar.
   - Si no → **regla de envío**, responder `WELCOME`, registrar al dron (`fd`, `estado = VIVO`, `ultimo_hb = time(NULL)`) y hacer `fork()`.
   - El **hijo** cierra lo que no usa, llama a `atender_dron()` y termina con `exit(0)`.
   - El **padre** conserva el socket del dron.

### TODO A.2 — El hijo cartero `atender_dron()` (10 pts)
1. Por cada línea, reconocer `HB <id> <T>` o `EVT <id> <seq> <T> <texto>` con `sscanf`, llenar un `Aviso` y escribirlo en la pipe. Ignorar líneas mal formadas y las que traen un `id` distinto al del dron.
2. Cuando el dron se desconecta (`recv <= 0`), enviar al padre un `Aviso` con `tipo = T_BYE`.

### TODO A.3 — El padre procesa los avisos (18 pts)
Por cada `Aviso` leído de la pipe:
- **`T_BYE`**: cerrar el socket del dron y dejar su estado en `LIBRE`.
- **`T_HB` o `T_EVT`**: **regla de recepción** con el `T` del aviso. Actualizar `ultimo_hb`. Si el dron estaba `CAIDO`, volver a `VIVO` e imprimir `DRON-<id> VOLVIÓ`.
- **`T_EVT`** además: **regla de envío** y responder:
  - si **hay quórum** y `seq == ultimo_seq + 1` → aplicar al log (`ultimo_seq = seq`), imprimir `[L=<reloj>] LOG DRON-<id> #<seq>: <texto>` y responder `ACK`;
  - si `seq <= ultimo_seq` (duplicado) → responder `ACK` sin volver a aplicarlo;
  - en cualquier otro caso → responder `NACK <ultimo_seq + 1>`.

### TODO A.4 — Heartbeats y quórum (12 pts)
En **cada vuelta** del bucle (llegue algo o venza el timeout):
1. Todo dron `VIVO` con `time(NULL) - ultimo_hb > TIMEOUT_HB` pasa a `CAIDO`: imprimir `DRON-<id> CAÍDO`.
2. Mantener una variable `hay_quorum` y, **solo cuando cambie**, imprimir `QUÓRUM OK (vivos=X/5)` o `SIN QUÓRUM (vivos=X/5)`. Hay quórum si `contar_vivos() >= QUORUM`.
3. Recoger los hijos terminados sin bloquear (`waitpid` con `WNOHANG`).

---

## Parte B — Dron (`dron.c`)  (40 pts, ~25 min)

El dron es **un solo proceso** (sin `fork`): un solo reloj de Lamport.

### TODO B.1 — Conexión y JOIN (12 pts)
1. Crear el socket y conectar a `ip:PORT`.
2. **Regla de envío**, enviar `JOIN <id> <T>`.
3. Recibir la respuesta y aplicar la **regla de recepción**. Si es `REJECT`, imprimir un mensaje, cerrar y terminar. Si es `WELCOME`, imprimir `Conectado`.

### TODO B.2 — Bucle con `select` y tareas periódicas (16 pts)
Bucle con `select()` sobre el socket y **timeout de 1 s**. En cada vuelta:
1. Si pasó **≥ 1 s** desde el último heartbeat → **regla de envío**, enviar `HB <id> <T>`.
2. Si pasaron **≥ 2 s** desde el último evento y quedan eventos (`seq_sig <= N_EVENTOS`) → **regla de envío**, enviar `EVT <id> <seq_sig> <T> <eventos[seq_sig-1]>`.
   - **Modo `falla`**: si `saltar` está activo y `seq_sig == 3`, **no lo envíes**: imprime que se perdió, desactiva `saltar` y avanza `seq_sig` a 4, como si se hubiera enviado.
3. `seq_sig` parte en 1 y **solo cambia con las respuestas de la Base** (B.3), salvo el caso `falla`.

### TODO B.3 — Respuestas de la Base (12 pts)
Cuando el socket tenga datos (puedes asumir que cada `recv` trae una sola línea):
1. `recv`; si devuelve `<= 0`, la Base se cayó: salir del bucle.
2. Leer `ACK <seq> <T>` o `NACK <seq> <T>` con `sscanf` y aplicar la **regla de recepción**.
3. `ACK s` → `seq_sig = s + 1` (si es mayor que el actual). `NACK s` → `seq_sig = s` (retroceder y reenviar desde ahí).
4. Cuando `seq_sig > N_EVENTOS`, imprimir **una vez** `Todos los eventos confirmados` y **seguir mandando heartbeats** (el dron sigue en vuelo hasta Ctrl+C).

---

## Cómo probarlo

Abre una terminal para la Base y una por dron:

```bash
./base
```
```bash
./dron 1 127.0.0.1
```
```bash
./dron 2 127.0.0.1 falla
```
```bash
./dron 3 127.0.0.1
```

| Prueba | Qué debería pasar |
|---|---|
| Solo 2 drones conectados | Sus `EVT` reciben `NACK` (**sin quórum**). Al conectar el 3º → `QUÓRUM OK` y los logs avanzan. |
| `./dron 3 ...` dos veces | El segundo recibe `REJECT`. |
| Dron con `falla` | La Base recibe #4 esperando #3 → `NACK 3` → el dron reenvía #3, #4 y #5, en orden. |
| `Ctrl+Z` en un dron (con 3 conectados) | ~3 s después: `DRON-x CAÍDO` y `SIN QUÓRUM`. |
| `fg` en ese dron | `DRON-x VOLVIÓ`, `QUÓRUM OK` y los eventos pendientes se confirman. |
| Timestamps del log | Siempre crecientes en la Base. |

---

## Parte C — Preguntas de desarrollo (sin puntaje, para el oral)

1. ¿Por qué el reloj de Lamport de la Base tiene que estar en el **padre** y no en los hijos? ¿Y por qué el dron **no** puede usar `fork()` para separar envío y recepción?
2. El log de la Base usa **número de secuencia** para detectar huecos. ¿En qué se parece a la replicación por log de **Primary-Backup**? ¿Qué hace un backup que recibe la entrada 7 cuando su último aplicado es la 5?
3. ¿Por qué el quórum es `N/2 + 1` y por qué conviene un número **impar** de nodos? ¿Qué problema evita (**split-brain**)?
4. Un `TIMEOUT_HB` de 1 s vs. uno de 30 s: ventajas y desventajas de cada uno. ¿Puede la Base distinguir un dron **caído** de uno **lento** o de una **red cortada**?
5. Si dos drones envían eventos **concurrentes**, ¿los timestamps de Lamport permiten detectarlo? ¿Qué usarías en su lugar y cómo compararías los vectores?
6. Si la Base se replicara con un backup: ¿qué cambia entre responder `ACK` **antes** o **después** de que el backup confirme? Relaciónalo con consistencia **fuerte vs. eventual** y con **CAP**.
7. En **Raft**: ¿qué hace un follower que deja de recibir heartbeats? ¿Por qué los timeouts son aleatorios? ¿Por qué un candidato sin la última entrada commiteada no puede ganar?

---

## Referencia de funciones

| Función | Parámetros | Descripción |
|---|---|---|
| `socket()` | `int domain, int type, int protocol` | Crea un socket y retorna su descriptor |
| `setsockopt()` | `int fd, int level, int opt, const void *val, socklen_t len` | Opciones del socket (`SO_REUSEADDR`) |
| `bind()` | `int fd, const struct sockaddr *addr, socklen_t len` | Asocia el socket a una IP y un puerto |
| `listen()` | `int fd, int backlog` | Deja el socket en modo escucha |
| `accept()` | `int fd, struct sockaddr *addr, socklen_t *len` | Acepta una conexión y retorna un nuevo descriptor |
| `connect()` | `int fd, const struct sockaddr *addr, socklen_t len` | Conecta con un servidor |
| `send()` / `recv()` | `int fd, void *buf, size_t len, int flags` | Envía / recibe por un socket |
| `pipe()` | `int fd[2]` | Crea una pipe: `fd[0]` lectura, `fd[1]` escritura |
| `read()` / `write()` | `int fd, void *buf, size_t n` | Lee / escribe en un descriptor (pipe) |
| `fork()` | `void` | Crea un proceso hijo |
| `waitpid()` | `pid_t pid, int *status, int options` | Espera a un hijo (`WNOHANG`: sin bloquear) |
| `select()` | `int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv` | Espera a que algún descriptor esté listo, o a que venza el timeout |
| `FD_ZERO` / `FD_SET` / `FD_ISSET` | `fd_set *set` / `int fd, fd_set *set` | Vaciar / agregar / consultar un `fd_set` |
| `time()` | `time_t *t` | Segundos actuales (`time(NULL)`) |
| `inet_pton()` | `int af, const char *src, void *dst` | IP en texto → binario |
| `htons()` | `uint16_t x` | Puerto al orden de bytes de red |
| `sscanf()` / `snprintf()` | `const char *str, const char *fmt, ...` / `char *str, size_t n, const char *fmt, ...` | Leer / escribir con formato |
| `strcmp()` / `strlen()` | `const char *s1, const char *s2` / `const char *s` | Comparar / largo de un string |
| `atoi()` | `const char *s` | String → `int` |
| `close()` | `int fd` | Cierra un descriptor |
| `exit()` / `perror()` | `int status` / `const char *msg` | Terminar el proceso / imprimir un error |
