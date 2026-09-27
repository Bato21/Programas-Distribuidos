# Solucionario: Control 01 (Base Lunar SELENE)

> ⚠️ Lee esto **después** de intentarlo tú en `../servidor.c` y `../sonda.c`.
> La teoría (sockets, strings, fork) está en [../GUIA.md](../GUIA.md). Aquí solo se explica **esta solución**.

Archivos:
- [servidor.c](servidor.c): `evaluar()` en la línea 61, A.1 en la 75, A.2 en la 110 y A.3 en la 174.
- [sonda.c](sonda.c): B.1 en la línea 39, B.2 en la 60 y B.3 en la 85.

## Recorrido de una sesión completa

Así se ve una conversación entre la sonda y la base, en orden:

```
SONDA (sonda.c)                                  BASE (servidor.c)
                                                 socket/bind/listen            [A.3]
socket + connect                  [B.1]  ─────►  accept → fork → hijo          [A.3]
send "CONNECT ARES-2\n"          [B.2]  ─────►  recv + sscanf + buscar id      [A.1]
recv → "HELLO" ? sigue : sale     [B.2]  ◄─────  send "HELLO ARES-2\n"          [A.1]
loop seq=1..8:                    [B.3]
  "%.2f" → checksum → send TELEM         ─────►  recv + sscanf (valor texto)    [A.2]
                                                 checksum igual? no → NACK
                                                 atof → evaluar → ACK (+ALERT)
  recv + printf                          ◄─────  send
  valor cambia, sleep(1)
close                                    ─────►  recv devuelve 0 → hijo exit(0)
```

## Salida real de la prueba

```
-> TELEM ARES-2 1 6.50 C9
<- ACK ARES-2 1
ALERT ARES-2 CRITICO valor=6.50 umbral=6.00
-> TELEM ARES-2 2 5.59 D1
<- ACK ARES-2 2
ALERT ARES-2 ALERTA valor=5.59 umbral=3.50
```

Checksum falso (`TELEM ARES-2 1 4.00 ZZ`) → `NACK ARES-2 1`. Sonda desconocida (`ARES-9`) → `REJECT ARES-9`.

---

## PARTE A: Servidor

### TODO A.1: conexión inicial (15 pts)

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

### TODO A.2: loop de telemetría + `evaluar()` (25 pts)

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

### TODO A.3: `main` con concurrencia (15 pts)

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

## PARTE B: Cliente (sonda)

### TODO B.1: socket + connect (15 pts)

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

### TODO B.2: CONNECT y respuesta (12 pts)

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

### TODO B.3: 8 lecturas (18 pts)

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
