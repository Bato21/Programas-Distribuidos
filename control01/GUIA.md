# Guía de estudio: Control 01 (Base Lunar SELENE)

> Objetivo: que puedas escribir `servidor.c` y `sonda.c` **solo, en 45 minutos y sin errores**.
> Todo sale de lo visto en clase: `ejemploPipe.c`, el chat v1 (`*-v1.c.bak`) y el chat v2 (`server-chat.c`, `client-chat.c`).
> La solución completa y probada está en [servidor.c](servidor.c) y [sonda.c](sonda.c).

---

## 0. Qué evaluó realmente el control

Aunque el contexto hable de la Luna, el control pide **exactamente lo mismo que el chat de clase**, con otros nombres:

| Lo que pide el control | Dónde lo viste en clase |
|---|---|
| Servidor TCP: `socket` → `bind` → `listen` → `accept` (A.3) | `server-chat-v1.c.bak`, líneas 30–58 |
| Cliente TCP: `socket` → `inet_pton` → `connect` (B.1) | `client-chat-v1.c.bak`, líneas 33–53 |
| Varios clientes a la vez con `fork()` (A.3) | `server-chat.c` (un hijo por cliente) |
| Cerrar los descriptores que no usa cada proceso | `ejemploPipe.c` y `server-chat.c` |
| `recv` + poner `'\0'` + quitar `'\n'` | todos los archivos del chat |
| Armar mensajes con `snprintf` | `server-chat.c` (`"Cliente %d: %s\n"`) |
| Leer los campos de un mensaje con `sscanf` | **nuevo**: lo explico en la sección 3 |
| Protocolo con comandos (`CONNECT`, `TELEM`…) | el comando `QUIT` del chat, pero con más comandos |

Lo nuevo de verdad son tres cosas: **`sscanf`**, **el checksum** y **la lógica de umbrales**. Todo lo demás es la misma receta de siempre.

---

## 1. Conceptos base (lo mínimo que hay que tener claro)

**Descriptor (fd).** Un número entero que el sistema operativo te entrega para referirte a algo abierto: un archivo, un pipe o un socket. `close(fd)` lo libera. Por ejemplo, `socket()` devuelve un fd, y `accept()` devuelve **otro** fd.

**Socket.** Es un "enchufe" de red. Con TCP (`SOCK_STREAM`) es un canal **bidireccional y confiable**: lo que mandas llega completo y en orden.

**IP y puerto.** La IP identifica la máquina (`127.0.0.1` es la misma máquina) y el puerto identifica el programa dentro de esa máquina (`7070`).

**Servidor y cliente.**
- El **servidor** se queda esperando en un puerto conocido (la base SELENE).
- El **cliente** toma la iniciativa y se conecta (la sonda ARES).

**Dos sockets en el servidor (esto confunde a todos):**
- `server_fd` es el "portero": solo sirve para **aceptar** conexiones. Por él nunca se envían datos.
- `client_fd` (el que devuelve `accept`) es la "línea telefónica" con **un** cliente. Por aquí se hacen `send` y `recv`.

**Orden de bytes (`htons`).** Cada CPU guarda los números a su manera, pero la red usa un orden fijo. Por eso el puerto siempre va como `htons(PORT)` (*host to network short*).

---

## 2. Las dos recetas que hay que saberse de memoria

### Receta SERVIDOR (6 pasos)

```c
int server_fd = socket(AF_INET, SOCK_STREAM, 0);          // 1. crear
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // (opcional, útil)

struct sockaddr_in addr = {                                 // 2. dirección
    .sin_family      = AF_INET,        // IPv4
    .sin_port        = htons(PORT),    // puerto en orden de red
    .sin_addr.s_addr = INADDR_ANY      // cualquier interfaz
};
bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));   // 3. amarrar al puerto
listen(server_fd, 10);                                      // 4. modo "escucha"

while (1) {
    int client_fd = accept(server_fd, NULL, NULL);          // 5. esperar cliente
    /* ... fork ... */                                      // 6. atender
}
```

Truco para acordarte: **S-B-L-A** ("Socket, Bind, Listen, Accept").

> `accept(server_fd, NULL, NULL)` es válido si no te importa la IP del cliente. En clase se usó `&client_addr, &client_len` para imprimirla; las dos formas son correctas.

### Receta CLIENTE (4 pasos)

```c
int sock = socket(AF_INET, SOCK_STREAM, 0);                // 1. crear
struct sockaddr_in srv = {                                  // 2. dirección del servidor
    .sin_family = AF_INET,
    .sin_port   = htons(PORT)
};
inet_pton(AF_INET, ip, &srv.sin_addr);                      // 3. "127.0.0.1" -> binario
connect(sock, (struct sockaddr *)&srv, sizeof(srv));       // 4. conectar
```

Truco: **S-I-C** ("Socket, Inet_pton, Connect"). El cliente **no** hace bind, listen ni accept.

### Verificar errores siempre

Toda llamada de sistema devuelve `< 0` si falla. El patrón de clase:

```c
if (x < 0) { perror("nombre"); return 1; }
```

`inet_pton` es la excepción: devuelve `1` si funcionó, así que se revisa con `<= 0`.

---

## 3. Strings en C: aquí se pierden la mayoría de los puntos

### 3.1 `recv` NO termina el string

```c
int n = recv(fd, buf, sizeof(buf) - 1, 0);   // deja 1 espacio para el '\0'
if (n <= 0) { /* 0 = el otro cerró; <0 = error */ }
buf[n] = '\0';                               // AHORA sí es un string
```

- ¿Por qué `sizeof(buf) - 1`? Porque si llegan 256 bytes, `buf[256]` quedaría fuera del arreglo. Es el **buffer overflow** que aparece en la lista de ataques de `client-chat.c`.
- ¿Por qué `n <= 0`? Porque `recv` devuelve `0` cuando el otro lado cerró la conexión. Si no lo revisas, el programa queda en un loop infinito.

### 3.2 Quitar el `'\n'`

```c
buf[strcspn(buf, "\r\n")] = '\0';
```

`strcspn` te dice en qué posición está el primer `\r` o `\n`, y en esa posición pones el fin de string. Si no hay ninguno, devuelve el largo y simplemente pisa el `'\0'` que ya estaba, así que es seguro.

### 3.3 `snprintf`: ESCRIBIR un mensaje (armarlo)

Funciona igual que `printf`, pero escribe en un buffer en vez de la pantalla:

```c
char msg[BUF_SIZE];
snprintf(msg, sizeof(msg), "TELEM %s %d %s %s\n", id, seq, valor_str, cs);
send(sock, msg, strlen(msg), 0);
```

Siempre `strlen(msg)` en el `send`, **nunca** `sizeof(msg)`: con `sizeof` enviarías los 256 bytes, basura incluida.

### 3.4 `sscanf`: LEER un mensaje (desarmarlo). Es la función clave del control

Es el inverso de `snprintf`: le das un string y un formato, y rellena tus variables.

```c
char id[8]; int seq; char valor_str[32]; char cs[3];

int campos = sscanf(buf, "TELEM %7s %d %31s %2s", id, &seq, valor_str, cs);
//                        ^^^^^ texto literal: debe calzar exacto
```

Reglas:
1. **El texto fijo del formato tiene que calzar.** Si `buf` empieza con `"HOLA"`, falla de inmediato.
2. **`%s` lee una palabra** (hasta el primer espacio).
3. **El número en `%7s` es el máximo de caracteres** y debe ser el tamaño del arreglo menos 1 (`id[8]` → `%7s`). Sin ese número, un atacante manda un id de 500 letras y rompe la memoria.
4. **`%d` necesita `&`** (`&seq`), porque es un `int`. Los arreglos (`id`, `cs`) **no** llevan `&`.
5. **Devuelve cuántos campos logró leer.** Así se valida el mensaje:
   ```c
   if (campos != 4) { /* mensaje mal formado */ }
   ```

### 3.5 `strcmp`: comparar strings

```c
if (strcmp(a, b) == 0)   // son IGUALES
```

¡`a == b` **no** compara strings en C, compara direcciones de memoria! Y ojo, que `strcmp` devuelve `0` cuando son iguales (al revés de lo que uno pensaría).

---

## 4. `fork()`: atender a varias sondas a la vez

### Qué hace

`fork()` **clona** el proceso. Desde esa línea hay dos procesos ejecutando el mismo código:

```c
pid_t pid = fork();
if (pid < 0)  { /* error */ }
if (pid == 0) { /* soy el HIJO */ }
else          { /* soy el PADRE; pid = número del hijo */ }
```

El hijo recibe una **copia** de todo: variables y también **descriptores abiertos** (por eso en `ejemploPipe.c` el hijo podía usar el pipe que creó el padre).

### Por qué el servidor lo necesita

Sin `fork`, el servidor atiende a ARES-1 dentro de su `while` y **no vuelve a `accept()`** hasta que ARES-1 termine. Las otras 3 sondas quedan esperando. Con `fork`:

```
PADRE:  accept -> fork -> accept -> fork -> accept -> ...   (solo recepcionista)
HIJO 1:         handle_sonda(ARES-1) ... exit
HIJO 2:                        handle_sonda(ARES-2) ... exit
```

### La regla de los `close` (la lección de `ejemploPipe.c`)

Cada proceso **cierra lo que no usa**:

| Proceso | Cierra | Por qué |
|---|---|---|
| HIJO | `server_fd` | no acepta clientes, solo atiende a uno |
| PADRE | `client_fd` | ya se lo pasó al hijo. Si no lo cierra, la conexión nunca se cierra de verdad y se le acaban los descriptores |

### Zombies y `waitpid`

Cuando un hijo termina, queda como "zombie" hasta que el padre lo recoge. Como el padre está ocupado en `accept`, se recogen los que ya terminaron **sin bloquearse**:

```c
while (waitpid(-1, NULL, WNOHANG) > 0);
```

- `-1`: cualquier hijo.
- `WNOHANG`: "si ninguno terminó todavía, no esperes". Si usaras `wait(NULL)` a secas, el padre se quedaría pegado esperando al primer hijo y dejaría de aceptar sondas.

---

## 5. El control resuelto, paso a paso

### Paso previo: leer el protocolo como un contrato

```
Sonda -> Base : CONNECT <id>\n
Base -> Sonda : HELLO <id>\n       o   REJECT <id>\n
Sonda -> Base : TELEM <id> <seq> <valor> <cs>\n      (x8)
Base -> Sonda : ACK <id> <seq>\n   o   NACK <id> <seq>\n
                ALERT <id> <nivel> <detalle>\n       (además del ACK, si hay alarma)
```

Por cada línea del protocolo, alguien hace un `snprintf` + `send` y el otro hace un `recv` + `sscanf`. **Ese es todo el control.** Antes de escribir código, anota en el borrador quién envía qué y en qué orden.

### ¿Qué es el checksum?

Es una "huella" del texto del valor: suma los códigos ASCII de cada carácter, se queda con el resto de dividir por 256 y lo escribe en 2 dígitos hexadecimales.

```
"6.50" -> '6'(54) + '.'(46) + '5'(53) + '0'(48) = 201 -> 201 % 256 = 201 -> "C9"
```

Sirve para detectar si el dato se corrompió en el camino: la sonda lo calcula y lo envía, la base lo **recalcula** y compara. Si no coinciden, responde `NACK`.

⚠️ **La trampa del control:** el checksum se calcula sobre el **texto**. Si la sonda manda `"6.50"` y la base lee con `%f` y luego vuelve a convertir a texto, podría quedar `"6.500000"`, con otro checksum. Por eso:
- **Sonda:** convierte el `float` a texto **una sola vez** (`"%.2f"`) y usa **ese mismo** string para el checksum y para el mensaje.
- **Base:** lee el valor como **string** (`%31s`), calcula el checksum sobre ese string y **después** lo convierte a número con `atof`.

---

### PARTE A: Servidor

#### TODO A.1: conexión inicial (15 pts)

Qué hay que hacer: recibir `CONNECT`, ver si el id existe en `umbrales[]` y responder `HELLO` o `REJECT`.

```c
int n = recv(fd, buf, sizeof(buf) - 1, 0);
if (n <= 0) return;
buf[n] = '\0';
buf[strcspn(buf, "\r\n")] = '\0';

char id[8];
if (sscanf(buf, "CONNECT %7s", id) != 1) {      // no era un CONNECT válido
    send(fd, "REJECT ?\n", 9, 0);
    return;
}

Umbral *u = NULL;                                // puntero a "mi" umbral
for (int i = 0; i < 4; i++) {
    if (strcmp(id, umbrales[i].id) == 0) { u = &umbrales[i]; break; }
}

char resp[BUF_SIZE];
if (u == NULL) {                                 // no está en la tabla
    snprintf(resp, sizeof(resp), "REJECT %s\n", id);
    send(fd, resp, strlen(resp), 0);
    return;                                      // se termina la atención
}
snprintf(resp, sizeof(resp), "HELLO %s\n", id);
send(fd, resp, strlen(resp), 0);
```

Idea clave: guardar **`u`** (un puntero al umbral de esta sonda). Así en A.2 no hay que volver a buscar: `u->alerta`, `u->critico` y `u->mayor` ya están a mano.

> `->` se usa con punteros a struct: `u->alerta` es lo mismo que `(*u).alerta`.

#### TODO A.2: loop de telemetría + `evaluar()` (25 pts)

Primero la función `evaluar`, **fuera** de `handle_sonda` (arriba de ella):

```c
const char *evaluar(const Umbral *u, float valor) {
    if (u->mayor) {                       // ARES-1,2,3: el peligro es que SUBA
        if (valor > u->critico) return "CRITICO";
        if (valor > u->alerta)  return "ALERTA";
    } else {                              // ARES-4: el peligro es que BAJE
        if (valor < u->critico) return "CRITICO";
        if (valor < u->alerta)  return "ALERTA";
    }
    return "NORMAL";
}
```

**¿Por qué se revisa CRÍTICO primero?** Con ARES-2, un valor de 7.0 es `> 3.5` (alerta) y también `> 6.0` (crítico). Si revisas la alerta primero, devuelves "ALERTA" y nunca llegas a "CRITICO". Siempre hay que revisar **del caso más grave al menos grave**.

**¿Para qué es el campo `mayor`?** Para no escribir un `if` distinto por cada sonda. La energía (ARES-4) es peligrosa cuando **baja**, así que se invierte la comparación. Un solo `if (u->mayor)` cubre las 4 sondas.

Ahora el loop:

```c
while (1) {
    n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) break;                           // la sonda terminó
    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    char t_id[8], valor_str[32], cs_rec[3], cs_calc[3];
    int seq;

    if (sscanf(buf, "TELEM %7s %d %31s %2s", t_id, &seq, valor_str, cs_rec) != 4) {
        snprintf(resp, sizeof(resp), "NACK %s 0\n", id);
        send(fd, resp, strlen(resp), 0);
        continue;                                // siguiente mensaje
    }

    checksum(valor_str, cs_calc);                // recalcular
    if (strcmp(cs_rec, cs_calc) != 0 || strcmp(t_id, id) != 0) {
        snprintf(resp, sizeof(resp), "NACK %s %d\n", id, seq);
        send(fd, resp, strlen(resp), 0);
        continue;
    }

    float valor = atof(valor_str);               // recién ahora a número
    const char *nivel = evaluar(u, valor);

    int len = snprintf(resp, sizeof(resp), "ACK %s %d\n", id, seq);
    if (strcmp(nivel, "NORMAL") != 0) {
        float limite = (strcmp(nivel, "CRITICO") == 0) ? u->critico : u->alerta;
        snprintf(resp + len, sizeof(resp) - len,
                 "ALERT %s %s valor=%.2f umbral=%.2f\n", id, nivel, valor, limite);
    }
    send(fd, resp, strlen(resp), 0);
}
```

Detalles que suman puntos:
- **`continue`** después de un `NACK`: salta el resto y vuelve a esperar el siguiente mensaje. Un dato corrupto no se evalúa.
- **`strcmp(t_id, id) != 0`**: si la sonda se conectó como ARES-1 pero manda `TELEM ARES-2 ...`, se rechaza. Es la "suplantación de identidad" que aparece en la lista de `client-chat.c`.
- **ACK + ALERT en un solo `send`**: `snprintf` devuelve cuántos caracteres escribió (`len`), y el segundo `snprintf` escribe **a continuación** (`resp + len`). Así la sonda recibe las dos líneas en un solo `recv` y no quedan desfasadas. Si en el control haces dos `send` separados, también se acepta: es lo que la mayoría esperaría.
- `(cond) ? a : b` es un if en una línea: "si es CRITICO usa `u->critico`, si no `u->alerta`".

#### TODO A.3: `main` con concurrencia (15 pts)

Es la receta del servidor más el `fork`:

```c
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd < 0) { perror("socket"); return 1; }

int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

struct sockaddr_in addr = {
    .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = INADDR_ANY
};
if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
if (listen(server_fd, 10) < 0) { perror("listen"); return 1; }

while (1) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); continue; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); close(client_fd); continue; }

    if (pid == 0) {                 // HIJO
        close(server_fd);
        handle_sonda(client_fd);
        close(client_fd);
        exit(0);                    // ¡IMPORTANTE!
    }
    close(client_fd);               // PADRE
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
```

⚠️ **El `exit(0)` del hijo es obligatorio.** Sin él, cuando el hijo termina `handle_sonda` sigue ejecutando el `while` y **también se pone a hacer `accept`**: terminas con procesos clonándose sin control.

---

### PARTE B: Cliente (sonda)

#### TODO B.1: socket + connect (15 pts)

Es la receta del cliente. La única diferencia con clase es que la IP viene en `ip` (`argv[2]`):

```c
int sock = socket(AF_INET, SOCK_STREAM, 0);
if (sock < 0) { perror("socket"); return 1; }

struct sockaddr_in srv = { .sin_family = AF_INET, .sin_port = htons(PORT) };
if (inet_pton(AF_INET, ip, &srv.sin_addr) <= 0) {
    fprintf(stderr, "IP inválida\n"); return 1;
}
if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
    perror("connect"); return 1;
}
```

#### TODO B.2: CONNECT y respuesta (12 pts)

```c
char buf[BUF_SIZE];
snprintf(buf, sizeof(buf), "CONNECT %s\n", id);
send(sock, buf, strlen(buf), 0);

int n = recv(sock, buf, sizeof(buf) - 1, 0);
if (n <= 0) { printf("Servidor cerró\n"); close(sock); return 1; }
buf[n] = '\0';

char tipo[16];
sscanf(buf, "%15s", tipo);              // primera palabra
if (strcmp(tipo, "HELLO") != 0) {       // fue REJECT (u otra cosa)
    printf("Rechazada: %s", buf);
    close(sock);
    return 1;
}
printf("Conectado: %s", buf);
```

Aquí se reutiliza `buf` para enviar y para recibir, y no hay problema porque se usan en momentos distintos.

#### TODO B.3: 8 lecturas (18 pts)

```c
for (int seq = 1; seq <= 8; seq++) {
    char valor_str[32], cs[3];
    snprintf(valor_str, sizeof(valor_str), "%.2f", valor);  // 1) float -> texto
    checksum(valor_str, cs);                                 // 2) cs del MISMO texto

    snprintf(buf, sizeof(buf), "TELEM %s %d %s %s\n", id, seq, valor_str, cs);
    send(sock, buf, strlen(buf), 0);                         // 3) enviar

    n = recv(sock, buf, sizeof(buf) - 1, 0);                 // 4) respuesta
    if (n <= 0) break;
    buf[n] = '\0';
    printf("%s", buf);                                       // 5) imprimir

    valor += ((rand() % 201) - 100) / 100.0f;                // 6) variar
    sleep(1);                                                // 7) esperar
}
close(sock);
```

- **"El valor varía en cada iteración"**: cualquier variación sirve. La más simple para el control es `valor += 1.5;`. La de arriba suma un número aleatorio entre −1.00 y +1.00: `rand() % 201` da de 0 a 200, al restar 100 queda entre −100 y 100, y al dividir por 100.0 queda entre −1.00 y 1.00. Hay que dividir por `100.0f` y no por `100`, porque `int / int` descarta los decimales.
- `srand(getpid())` antes del `for` hace que cada sonda tenga números aleatorios distintos.
- El `for` va de **1 a 8 inclusive** (`seq <= 8`), porque el enunciado dice "seq 1–8".

---

## 6. Errores típicos (revisa esta lista antes de entregar)

1. ❌ `recv(fd, buf, sizeof(buf), 0)` sin el `-1` → no queda espacio para el `'\0'`.
2. ❌ Olvidar `buf[n] = '\0'` → `strcmp`/`sscanf` leen basura.
3. ❌ `if (buf == "HELLO")` → hay que usar `strcmp(...) == 0`.
4. ❌ `send(fd, msg, sizeof(msg), 0)` → debe ser `strlen(msg)`.
5. ❌ `sscanf(..., "%d", seq)` sin `&` → se cae el programa.
6. ❌ `sscanf` con `%s` sin límite (`%7s`) → buffer overflow.
7. ❌ Leer el valor con `%f` y calcular el checksum sobre otro texto → NACK en todo.
8. ❌ Revisar ALERTA antes que CRÍTICO → nunca sale CRÍTICO.
9. ❌ Olvidar que ARES-4 es al revés (`<`) → usa `u->mayor`.
10. ❌ Hijo sin `exit(0)` → explosión de procesos.
11. ❌ No hacer `close(client_fd)` en el padre / `close(server_fd)` en el hijo.
12. ❌ `wait(NULL)` bloqueante en el loop del padre → atiende solo a una sonda a la vez.
13. ❌ Olvidar `htons(PORT)`.
14. ❌ Olvidar el `\n` al final de cada mensaje del protocolo.

---

## 7. Cómo probarlo en tu PC

```bash
gcc -Wall -Wextra -g -o servidor servidor.c
gcc -Wall -Wextra -g -o sonda sonda.c
./servidor
```

En **otras** terminales:

```bash
./sonda ARES-1 127.0.0.1 -31     # empieza en ALERTA (> -40)
./sonda ARES-2 127.0.0.1 6.5     # empieza en CRITICO (> 6.0)
./sonda ARES-3 127.0.0.1 700     # NORMAL
./sonda ARES-4 127.0.0.1 16      # ALERTA (< 30), cerca de CRITICO (< 15)
./sonda ARES-9 127.0.0.1 1       # REJECT
```

Para probar un checksum malo "a mano" (escribes tú los mensajes):

```bash
nc 127.0.0.1 7070
```

Y escribes: `CONNECT ARES-2`, luego `TELEM ARES-2 1 4.00 ZZ` → debe responder `NACK ARES-2 1`.

---

## 8. Plan para que lo hagas solo

Hazlo **en orden** y sin mirar la solución hasta terminar cada paso:

1. **Día 1: recetas.** Escribe en una hoja en blanco la receta S-B-L-A y S-I-C. Compara con la sección 2. Repite hasta que te salgan perfectas 2 veces seguidas.
2. **Día 1: strings.** Escribe un `main` que haga `sscanf("TELEM ARES-1 3 -25.50 1F", ...)` e imprima los 4 campos y el valor de retorno. Luego prueba con un string mal formado y fíjate cómo cambia el retorno.
3. **Día 2: `evaluar`.** Escríbela y pruébala con: ARES-1 −35 (ALERTA), −25 (CRITICO), −50 (NORMAL); ARES-4 20 (ALERTA), 10 (CRITICO), 50 (NORMAL).
4. **Día 2: servidor sin fork.** A.1 + A.2 con un solo cliente (como el chat v1). Prueba con `nc`.
5. **Día 3: agrega el fork** (A.3) y conecta 4 sondas a la vez.
6. **Día 3: cliente** B.1 → B.2 → B.3.
7. **Simulacro:** borra todo, ponle un timer de **45 minutos** y escribe los dos archivos desde el esqueleto del enunciado. Compila con `-Wall -Wextra`: **cero warnings**.

### Variantes para practicar (pueden aparecer en otro control)
- Rechazar con `NACK` si el `seq` no es el siguiente esperado (1, 2, 3…).
- Que la base cuente cuántas alertas tuvo cada sonda y lo imprima al desconectarse.
- Que la sonda, si recibe `NACK`, reenvíe la misma lectura (mismo `seq`).
- Que la sonda termine antes si recibe un `ALERT ... CRITICO`.
