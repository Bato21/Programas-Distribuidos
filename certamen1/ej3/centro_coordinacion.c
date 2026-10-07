/*
 * CERTAMEN PASADO - EJERCICIO 3: CENTRO DE COORDINACIÓN (servidor TCP)
 *
 * Es el mismo esqueleto del servidor del Control 1:
 *   socket -> bind -> listen -> while(1){ accept -> fork -> hijo atiende }
 *
 * Lo nuevo es lo que hace el hijo con cada alerta:
 *   1. Parsear el mensaje con campos separados por '|'
 *   2. Clasificar el evento (KILONOVA, SUPERNOVA, ASTEROID, UNKNOWN)
 *   3. Responder al observatorio con un comando ToO o con un aviso
 *
 * Compilar: gcc -Wall -o centro_coordinacion centro_coordinacion.c
 * Ejecutar: ./centro_coordinacion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 7000
#define BUF_SIZE 256


// Atiende a UN observatorio. Lo ejecuta el proceso hijo.
void handle_observatorio(int client_fd){

    char buf[BUF_SIZE];
    char resp[BUF_SIZE];
    ssize_t n;

    // Mismo patrón que el read de los pipes: seguimos mientras lleguen bytes.
    // recv devuelve 0 cuando el observatorio cierra el socket.
    while ((n = recv(client_fd, buf, sizeof(buf) - 1, 0)) > 0){

        buf[n] = '\0';
        // Quitamos el '\n' del final para imprimir limpio
        buf[strcspn(buf, "\n")] = '\0';

        // ---------- 1. Parseo ----------
        // Formato: OBSERVATORIO|RA|DEC|MAGNITUD|TIPO_EVENTO|TIMESTAMP|PRIORIDAD
        // Ejemplo: OBS_CHILE|123.45|-23.67|15.80|KILONOVA|1760000000|5
        //
        // %31[^|] significa: "lee texto hasta encontrar un '|'" (máx 31 chars).
        // Es la forma de separar campos cuando el separador NO es un espacio.
        // Los '|' que van entre los % son literales: sscanf los "salta".
        char obs[32], tipo[16];
        float ra, dec, mag;
        long ts;
        int prio;

        int campos = sscanf(buf, "%31[^|]|%f|%f|%f|%15[^|]|%ld|%d",
                            obs, &ra, &dec, &mag, tipo, &ts, &prio);

        // sscanf devuelve cuántos campos logró leer: deben ser los 7
        if (campos != 7){
            printf("[CENTRO] Mensaje mal formado: %s\n", buf);
            snprintf(resp, sizeof(resp), "ERROR|FORMATO\n");
            send(client_fd, resp, strlen(resp), 0);
            continue;
        }

        printf("[CENTRO] ALERTA RECIBIDA de %s:\n", obs);
        printf("  Coordenadas: RA=%.2f, DEC=%.2f\n", ra, dec);
        printf("  Magnitud: %.2f  Tipo: %s  Prioridad: %d\n", mag, tipo, prio);

        // ---------- 2. Clasificación ----------
        // Reglas del enunciado (magnitud MENOR = más brillante)
        if (strcmp(tipo, "KILONOVA") == 0 && mag < 16){
            printf("[CENTRO] ANÁLISIS: KILONOVA - MÁXIMA PRIORIDAD -> ToO a TODOS\n");
            snprintf(resp, sizeof(resp), "ToO|RA=%.2f|DEC=%.2f|TIPO=KILONOVA\n", ra, dec);
        }
        else if (strcmp(tipo, "SUPERNOVA") == 0 && mag < 17){
            printf("[CENTRO] ANÁLISIS: SUPERNOVA -> ToO a 3 observatorios\n");
            snprintf(resp, sizeof(resp), "ToO|RA=%.2f|DEC=%.2f|TIPO=SUPERNOVA\n", ra, dec);
        }
        else if (strcmp(tipo, "ASTEROID") == 0){
            printf("[CENTRO] ANÁLISIS: ASTEROID -> seguimiento rutinario\n");
            snprintf(resp, sizeof(resp), "RUTINA|TIPO=ASTEROID\n");
        }
        else {
            printf("[CENTRO] ANÁLISIS: %s -> archivado para análisis posterior\n", tipo);
            snprintf(resp, sizeof(resp), "ARCHIVADO|TIPO=%s\n", tipo);
        }

        // ---------- 3. Respuesta ----------
        // SIMPLIFICACIÓN: el ToO se devuelve al MISMO observatorio que avisó.
        // Mandarlo a los OTROS observatorios no es directo: cada uno lo atiende
        // un hijo distinto y los hijos NO comparten variables (igual que en P2,
        // fork copia la memoria). Para eso los hijos tendrían que avisarle al
        // padre (por ejemplo con un pipe) y el padre reenviar a todos.
        if (send(client_fd, resp, strlen(resp), 0) < 0){
            perror("send");
            return;
        }
        printf("[CENTRO] -> Enviado a %s: %s", obs, resp);
    }

    if (n < 0){
        perror("recv");
    }
    printf("[CENTRO] Observatorio desconectado\n");
}


int main(){

    // ---------- Servidor TCP: idéntico al Control 1 ----------
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0){
        perror("socket");
        return 1;
    }

    // Permite reiniciar el servidor sin esperar "Address already in use"
    // (viene en el esqueleto de la guía, ejercicio S1)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 10) < 0){
        perror("listen");
        return 1;
    }

    printf("[CENTRO] Iniciando en puerto %d...\n", PORT);
    printf("[CENTRO] Esperando conexiones de observatorios...\n");

    while (1){
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0){
            perror("accept");
            // Un accept fallido no debe botar el servidor completo
            continue;
        }

        printf("[CENTRO] Nuevo observatorio conectado\n");

        pid_t pid = fork();

        if (pid < 0){
            perror("fork");
            close(client_fd);
            continue;
        }

        // Hijo: atiende al observatorio y muere
        if (pid == 0){
            close(server_fd);           // el hijo no acepta clientes
            handle_observatorio(client_fd);
            close(client_fd);
            exit(0);
        }

        // Padre: el cliente lo atiende el hijo, así que cierra su copia
        close(client_fd);
        // Recoge hijos terminados (evita zombis)
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    close(server_fd);
    return 0;
}
