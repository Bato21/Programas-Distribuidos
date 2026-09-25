/*
 * ============================================================
 *  CHAT DISTRIBUIDO - Versión 2: broadcast con fork + pipe
 * ============================================================
 *
 *  Lo que hace esta versión:
 *    - Escucha en TCP puerto 8080
 *    - Espera a que se conecten MAX_CLIENTS clientes
 *    - Crea UN pipe y hace fork() una vez por cliente
 *    - Cada hijo lee "su" socket y escribe el mensaje al pipe
 *    - El padre lee del pipe y reenvía a TODOS los demás
 *      (broadcast: si Ana escribe, el resto lo ve)
 *
 *  ¿Por qué hace falta el pipe?
 *    Después de fork() cada hijo tiene su PROPIA memoria. El hijo
 *    de Ana no puede tocar el socket de Bob. El pipe es el único
 *    canal por el que los hijos le pasan datos al padre, que sí
 *    tiene todos los sockets (los aceptó él antes de forkear).
 *
 *  Compilar:  gcc -Wall -Wextra -g -o server server-chat.c
 *  Ejecutar:  ./server
 *
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        8080
#define BUF_SIZE    512
#define MAX_CLIENTS 3     /* subir este número = más clientes, nada más */

/*
 * Lo que viaja por el pipe. Es de tamaño FIJO a propósito:
 * un write() de menos de PIPE_BUF (4096 bytes) es atómico, así que
 * dos hijos escribiendo a la vez nunca mezclan sus mensajes.
 * Y como todos los write/read son de sizeof(struct mensaje), el padre
 * siempre sabe dónde empieza y termina cada uno.
 */
struct mensaje {
    int  origen;      /* índice del cliente que lo envió (0..MAX_CLIENTS-1) */
    int  desconecta;  /* 1 = este cliente se fue del chat */
    char texto[BUF_SIZE];
};

int main(void) {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); /* permite reusar puerto inmediatamente */

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,      /* AF_INET = IPv4 */
        .sin_port        = htons(PORT),  /* htons: host->network byte order */
        .sin_addr.s_addr = INADDR_ANY    /* aceptar de cualquier IP */
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) { perror("listen"); return 1; }

    printf("[SERVER] Escuchando en puerto %d...\n", PORT);
    printf("[SERVER] Esperando %d clientes para abrir el chat.\n\n", MAX_CLIENTS);

    /* ---------- 1) Aceptar todos los clientes ---------- */
    int clients[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        clients[i] = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (clients[i] < 0) { perror("accept"); return 1; }

        printf("[SERVER] Cliente %d conectado desde %s:%d\n",
               i + 1, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }

    printf("[SERVER] Chat abierto.\n\n");

    /* ---------- 2) El pipe compartido ---------- */
    int pipe_fd[2];  /* [0]=read (padre), [1]=write (hijos) */
    if (pipe(pipe_fd) == -1) { perror("pipe"); return 1; }

    /*
     * Vaciar stdout ANTES de fork(). Si stdout no es un terminal está
     * "full-buffered": los printf de arriba siguen en memoria. fork()
     * copiaría ese buffer a cada hijo y el texto saldría duplicado
     * (una vez por proceso) al terminar.
     */
    fflush(stdout);

    /* ---------- 3) Un hijo por cliente ---------- */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pid_t pid = fork();
        if (pid == -1) { perror("fork"); return 1; }

        if (pid == 0) {
            /* ===== HIJO i: solo lee el cliente i y escribe al pipe ===== */
            close(pipe_fd[0]);   /* no lee del pipe */
            close(server_fd);    /* no acepta conexiones */
            for (int j = 0; j < MAX_CLIENTS; j++)
                if (j != i) close(clients[j]);  /* los sockets ajenos no le sirven */

            char buf[BUF_SIZE];
            struct mensaje m;
            m.origen = i;

            while (1) {
                int n = recv(clients[i], buf, sizeof(buf) - 1, 0);
                if (n <= 0) break;                     /* cliente cerró */

                buf[n] = '\0';                         /* igual que en ejemploPipe.c */
                if (buf[n-1] == '\n') buf[n-1] = '\0'; /* quitar el '\n' final */

                if (strcmp(buf, "QUIT") == 0) {
                    send(clients[i], "OK Hasta luego\n", 15, 0);
                    break;
                }

                m.desconecta = 0;
                snprintf(m.texto, sizeof(m.texto), "%s", buf);
                write(pipe_fd[1], &m, sizeof(m));      /* -> al padre */
            }

            /* avisarle al padre que este cliente se fue */
            m.desconecta = 1;
            m.texto[0] = '\0';
            write(pipe_fd[1], &m, sizeof(m));

            close(clients[i]);
            close(pipe_fd[1]);
            exit(0);
        }
    }

    /* ===== PADRE: el que hace el broadcast ===== */

    /*
     * CLAVE (es la lección de ejemploPipe.c): el padre tiene que cerrar
     * SU copia del extremo de escritura. Si no lo hace, cuando todos los
     * hijos terminen el read() de abajo no vería nunca EOF y se quedaría
     * bloqueado para siempre.
     */
    close(pipe_fd[1]);

    struct mensaje m;
    char salida[BUF_SIZE + 64];
    int activos = MAX_CLIENTS;

    while (read(pipe_fd[0], &m, sizeof(m)) == sizeof(m)) {

        if (m.desconecta) {
            printf("[SERVER] Cliente %d se desconectó.\n", m.origen + 1);
            snprintf(salida, sizeof(salida), "*** Cliente %d salió del chat ***\n", m.origen + 1);
            close(clients[m.origen]);
            clients[m.origen] = -1;   /* -1 = ranura libre */
            activos--;
        } else {
            printf("[SERVER] Cliente %d: \"%s\"\n", m.origen + 1, m.texto);
            snprintf(salida, sizeof(salida), "Cliente %d: %s\n", m.origen + 1, m.texto);
        }

        /* broadcast: a todos menos al que lo escribió */
        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (clients[j] < 0)  continue;   /* ya se fue */
            if (j == m.origen)   continue;   /* no le devolvemos su propio mensaje */
            send(clients[j], salida, strlen(salida), 0);
        }

        if (activos == 0) break;
    }

    close(pipe_fd[0]);
    for (int j = 0; j < MAX_CLIENTS; j++)
        if (clients[j] >= 0) close(clients[j]);
    close(server_fd);

    for (int i = 0; i < MAX_CLIENTS; i++) wait(NULL);  /* recoger a los hijos */

    printf("[SERVER] Terminado.\n");
    return 0;
}
