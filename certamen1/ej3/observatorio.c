/*
 * CERTAMEN PASADO - EJERCICIO 3: OBSERVATORIO (pipes + cliente TCP)
 *
 * Un observatorio = 2 procesos del MISMO programa:
 *
 *   TELESCOPIO (hijo)                 ANALIZADOR (padre)                  CENTRO
 *        |  ---- tel_a_ana (pipe) --->      |  ---- alerta (socket) --->     |
 *        |  <--- ana_a_tel (pipe) ----      |  <--- ToO    (socket) ----     |
 *
 * - Pipes:   comunicación LOCAL (misma máquina, mismo programa)
 * - Socket:  comunicación por RED con el Centro
 *
 * Flujo de cada vuelta ("por turnos", así nunca se mezclan mensajes):
 *   1. Telescopio genera una detección y la manda por pipe
 *   2. Analizador la clasifica y manda la alerta al Centro por socket
 *   3. Centro responde (ToO o aviso) por socket
 *   4. Analizador reenvía esa respuesta al Telescopio por pipe
 *   5. Telescopio la imprime y genera la siguiente detección
 *
 * Compilar: gcc -Wall -o observatorio observatorio.c
 * Ejecutar: ./observatorio OBS_CHILE      (con el centro ya corriendo)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 7000
#define IP "127.0.0.1"
#define BUF_SIZE 256
#define N_DETECCIONES 5


// Argumentos: argc representa el número de argumentos y argv es un array de cadenas que contiene los argumentos pasados al programa. 
// El primer argumento (argv[0]) es el nombre del programa, y los siguientes son los argumentos proporcionados por el usuario. 
// En este caso, se espera que el usuario proporcione un argumento adicional que representa el nombre del observatorio.
int main(int argc, char *argv[]){

    // Nombre del observatorio por argumento: ./observatorio OBS_CHILE
    if (argc != 2){
        printf("Uso: %s <NOMBRE_OBSERVATORIO>\n", argv[0]);
        return 1;
    }
    char *nombre = argv[1];

    printf("[%s] Iniciando observatorio...\n", nombre);

    // ---------- 1. Los 2 pipes, ANTES del fork (igual que P2) ----------
    int tel_a_ana[2];   // Telescopio escribe -> Analizador lee
    int ana_a_tel[2];   // Analizador escribe -> Telescopio lee

    if (pipe(tel_a_ana) == -1){
        perror("pipe");
        return 1;
    }
    if (pipe(ana_a_tel) == -1){
        perror("pipe");
        return 1;
    }

    // ---------- 2. fork: hijo = TELESCOPIO, padre = ANALIZADOR ----------
    pid_t pid = fork();

    if (pid == -1){
        perror("fork");
        return 1;
    }

    // =====================================================================
    //                         TELESCOPIO (hijo)
    // =====================================================================
    if (pid == 0){
        // Cerramos lo que NO usa: lectura de tel_a_ana, escritura de ana_a_tel
        close(tel_a_ana[0]);
        close(ana_a_tel[1]);

        // Semilla distinta por proceso (si lanzas 2 observatorios en el mismo
        // segundo, con time(NULL) generarían las mismas detecciones)
        srand(getpid());

        printf("[TELESCOPIO] Iniciado - Comenzando escaneo del cielo\n");

        char msg[BUF_SIZE];
        char resp[BUF_SIZE];
        ssize_t n;

        for (int i = 0; i < N_DETECCIONES; i++){
            // Detección aleatoria (misma fórmula de P1: entre 0.0 y 1.0, luego escalar)
            float ra  = (float)rand() / RAND_MAX * 360.0;          // 0 a 360
            float dec = (float)rand() / RAND_MAX * 180.0 - 90.0;   // -90 a +90
            float mag = (float)rand() / RAND_MAX * 7.0 + 14.0;     // 14 a 21

            printf("[TELESCOPIO] Detección: RA=%.2f, DEC=%.2f, Mag=%.2f\n", ra, dec, mag);

            // Paso 1: detección -> Analizador (pipe)
            snprintf(msg, sizeof(msg), "DET %.2f %.2f %.2f\n", ra, dec, mag);
            write(tel_a_ana[1], msg, strlen(msg));
            printf("[TELESCOPIO] -> Enviando a Analizador (pipe)\n");

            // Paso 5: esperamos la respuesta del Analizador (pipe).
            // read se BLOQUEA hasta que llegue algo: eso nos sincroniza.
            n = read(ana_a_tel[0], resp, sizeof(resp) - 1);
            if (n <= 0){
                // 0 = el Analizador cerró el pipe (por ej. se cayó el socket)
                break;
            }
            resp[n] = '\0';
            resp[strcspn(resp, "\n")] = '\0';   // quitamos el '\n' (como en el Control 1)

            // ¿Es un ToO? Sacamos la primera palabra (hasta el primer '|')
            // y la comparamos, igual que el HELLO de la sonda del Control 1
            char palabra[16];
            sscanf(resp, "%15[^|]", palabra);

            if (strcmp(palabra, "ToO") == 0){
                printf("[TELESCOPIO] ToO RECIBIDO - Interrumpiendo escaneo: %s\n", resp);
                printf("[TELESCOPIO] Observación completada\n");
            }
            else {
                printf("[TELESCOPIO] Sin ToO (%s) - Sigo escaneando\n", resp);
            }

            sleep(2);
        }

        // Al cerrar la escritura, el read del Analizador devuelve 0 y termina
        close(tel_a_ana[1]);
        close(ana_a_tel[0]);
        exit(0);
    }

    // =====================================================================
    //                         ANALIZADOR (padre)
    // =====================================================================

    // Cerramos lo que NO usa: escritura de tel_a_ana, lectura de ana_a_tel
    close(tel_a_ana[1]);
    close(ana_a_tel[0]);

    // ---------- 3. Cliente TCP: idéntico a la sonda del Control 1 ----------
    printf("[ANALIZADOR] Iniciado - Conectando a Centro en %s:%d\n", IP, PORT);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("socket");
        return 1;
    }

    struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
    };

    if (inet_pton(AF_INET, IP, &srv.sin_addr) <= 0){
        perror("inet_pton");
        return 1;
    }

    // Si falla la conexión, cerramos los pipes y esperamos al hijo (Telescopio)
    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) < 0){
        perror("connect");
        // Cerramos los pipes: el Telescopio verá read = 0 y terminará
        close(tel_a_ana[0]);
        close(ana_a_tel[1]);
        wait(NULL);
        return 1;
    }

    printf("[ANALIZADOR] Conectado exitosamente al Centro\n");

    char det[BUF_SIZE];
    char alerta[BUF_SIZE];
    char resp[BUF_SIZE];
    ssize_t n;

    // ---------- 4. Loop: pipe -> socket -> socket -> pipe ----------
    // Mismo patrón de P1/P2: mientras el Telescopio siga mandando
    while ((n = read(tel_a_ana[0], det, sizeof(det) - 1)) > 0){

        det[n] = '\0';
        printf("[ANALIZADOR] Recibido de Telescopio (pipe)\n");

        float ra, dec, mag;
        sscanf(det, "DET %f %f %f", &ra, &dec, &mag);

        // Análisis local: según el brillo decidimos tipo y prioridad
        // (magnitud MENOR = objeto más brillante = más interesante)
        char tipo[16];
        int prio;

        if (mag < 16){
            strcpy(tipo, "KILONOVA");
            prio = 5;
        }
        else if (mag < 17){
            strcpy(tipo, "SUPERNOVA");
            prio = 4;
        }
        else if (mag < 19){
            strcpy(tipo, "ASTEROID");
            prio = 3;
        }
        else {
            strcpy(tipo, "UNKNOWN");
            prio = 1;
        }

        printf("[ANALIZADOR] Análisis local: Posible %s (prioridad %d)\n", tipo, prio);

        // Paso 2: alerta -> Centro (socket), con el formato del enunciado
        // OBSERVATORIO|RA|DEC|MAGNITUD|TIPO_EVENTO|TIMESTAMP|PRIORIDAD
        snprintf(alerta, sizeof(alerta), "%s|%.2f|%.2f|%.2f|%s|%ld|%d\n",
                 nombre, ra, dec, mag, tipo, (long)time(NULL), prio);

        if (send(sock, alerta, strlen(alerta), 0) < 0){
            perror("send");
            break;
        }
        printf("[ANALIZADOR] -> Enviando alerta a Centro (socket)\n");

        // Paso 3: respuesta del Centro (socket)
        n = recv(sock, resp, sizeof(resp) - 1, 0);
        if (n <= 0){
            printf("[ANALIZADOR] El Centro cerró la conexión\n");
            break;
        }
        resp[n] = '\0';
        printf("[ANALIZADOR] Respuesta del Centro (socket): %s", resp);

        // Paso 4: respuesta -> Telescopio (pipe), tal cual llegó
        write(ana_a_tel[1], resp, strlen(resp));
        printf("[ANALIZADOR] -> Reenviando a Telescopio (pipe)\n");
    }

    // ---------- 5. Cierre ordenado ----------
    close(sock);
    close(tel_a_ana[0]);
    close(ana_a_tel[1]);
    wait(NULL);     // esperamos al Telescopio (evita zombi)

    printf("[%s] Observatorio finalizado\n", nombre);
    return 0;
}
