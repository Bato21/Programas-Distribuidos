#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     7070
#define BUF_SIZE 256

char *checksum(const char *valor, char out[3]) {
    unsigned int s = 0;
    for (int i = 0; valor[i]; i++) s += (unsigned char)valor[i];
    snprintf(out, 3, "%02X", s % 256);
    return out;
}

typedef struct {
    char  id[8];
    float alerta;
    float critico;
    int   mayor;   /* 1: alerta si valor > umbral | 0: si valor < umbral */
} Umbral;

Umbral umbrales[] = {
    {"ARES-1", -40, -30, 1},
    {"ARES-2", 3.5, 6.0, 1},
    {"ARES-3", 750, 900, 1},
    {"ARES-4",  30,  15, 0},
};

/* >>> SIGUIENTE PASO: escribir aqui la funcion evaluar(Umbral *u, float valor)
 *     que devuelva "NORMAL", "ALERTA" o "CRITICO".
 *     Partir solo con el caso mayor == 1 (revisar CRITICO antes que ALERTA).
 *     Probar con ARES-1: -50.00, -35.00 y -25.00. */

void handle_sonda(int fd) {
    char buf[BUF_SIZE];

    /* TODO A.1 */

    // n Representa la cantidad de bytes recibida por recv, fd es por donde, buf el mensaje, cuanto es el maximo que mide buf, 0 SIEMPRE
    // Si el mensaje no es valido, retornamos error
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0){
        printf("ERROR: No se logró establecer conexión con la sonda\n");
        return;
    }
    printf("Llegaron %d bytes\n", n);

    //n es la cantidad de bytes que llegaron, por ende en la posicion n del string agregamos el caracter de fin
    buf[n] = '\0';

    //strcspn recorre buf (el mensaje) y busca el primer \r o \n y devuelve su posicion, nosotros tomamos esa posicion y la cambiamos por \0
    buf[strcspn(buf, "\r\n")] = '\0';
    printf("Mensaje: [%s]\n", buf);

    // Creamos la variable id que es texto de hasta 7 caracteres 'ARES-x'
    // Creamos la variable campos, toma el mensaje buf y lo separa en x variables, la variable mide cuantas recuperamos
    // Imprimimos la cantidad de campos que recuperamos del mensaje y luego su valor
    // Si no logramos identificar la sonda (id no se llena y campos = 0) retornamos error
    char id[8];
    int campos = sscanf(buf, "CONNECT %7s", id);
    if (campos != 1){
        printf("ERROR: No se pudo identificar la sonda\n");
        return;
    }
    printf("campos = %d, id = [%s]\n", campos, id);

    // Si el mensaje es recibido y tiene los campos necesarios, verificamos que la sonda existe

    // Sacamos el total de sondas disponibles para luego poder recorrer el arreglo y saber cuando parar
    // Tomamos el tamaño en memoria de la tabla entera, y lo dividimos por el tamaño de un campo para saber cuantos campos hay en total
    int total = sizeof(umbrales) / sizeof(umbrales[0]);

    // Puntero para consultar el valor de la sonda que estamos guardando
    Umbral *u = NULL;

    for (int i = 0; i < total; i++) {
        printf("Comparando con %s\n", umbrales[i].id);
        if (strcmp(umbrales[i].id, id) == 0) {
            printf("¡Encontrada en la posicion %d!\n", i);
            // Le asignamos a u (el puntero) la dirección de donde esta guardado el valor de la sonda que vamos a guardar
            u = &umbrales[i];
            break;
        }

    }


    // Armamos el mensaje de respuesta para la sonda
    char resp[BUF_SIZE];

    // Si no encontramos la sonda como valida rechazamos la conexion
    if (u == NULL){
        printf("ERROR: Se rechazó la conexión, id de sonda no encontrado\n");

        // snprintf deja armada la estructura, en que variable se guarda, cuanto cabe para no pasarse, la estructura del mensaje
        snprintf(resp, sizeof(resp), "REJECT %s\n", id);

        // send manda un mensaje, funciona igual que recv, a donde se manda el mensaje, el mensaje a mandar, cuanto mide el mensaje que escribimos, SIEMPRE 0
        send(fd, resp, strlen(resp), 0);   
        return;
    }

    snprintf(resp, sizeof(resp), "HELLO %s\n", id);
    send(fd, resp, strlen(resp), 0);
    

    /* TODO A.2 */

    // La sonda empieza a mandar lecturas TELEM ARES-1 1 -35.50 C4 en bucle
    while (1) {
        // Recibimos el mensaje de la sonda
        n = recv(fd, buf, sizeof(buf) -1, 0);

        // Si el mensaje esta corrupto o vacío cortamos la conexión
        if (n <= 0) {
            printf("ERROR: Conexión cortada\n");
            break;
        }

        // Agregamos el terminador al final del mensaje que recibimos
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        printf("Telemetría recibida: [%s]\n", buf);

        // Preparamos el mensaje para leerlo
        // Creamos las variables que vamos a recibir
        char idtelem[8];
        int seq;
        char valor[10];
        char cs[3];

        // Rellenamos las variables con los valores del texto tomando en cuenta EXACTAMENTE como llegan
        campos = sscanf(buf, "TELEM %7s %d %9s %2s", idtelem, &seq, valor, cs);

        // Si no recuperamos la cantidad necesaria de campos mandamos error y cerramos conexión
        if (campos != 4) {
            printf("ERROR: No se pudo recuperar la telemetría\n");
            continue;
        }

        printf("Campos: %d id: %s seq: %d valor: %s cs: %s\n", campos, idtelem, seq, valor, cs);

        // Checkeamos con el checksum
        char check[3];
        checksum(valor, check);
        printf("Valor recibido: %2s\nValor calculado: %2s\n", cs, check);

        // Verificamos que los resultados sean igual
        // Si no lo son mandamos el mensaje correspondiente
        if (strcmp(check, cs) != 0 || strcmp(idtelem,id) != 0){
            printf("ERROR: Telemetría corrupta\n");
            snprintf(resp, sizeof(resp), "NACK %s %d\n", id, seq);
            send(fd, resp, strlen(resp), 0);
            continue;
        }

        // Si es válido se envía el mensaje correspondiente
        printf("Telemetría válida\n");
        snprintf(resp, sizeof(resp), "ACK %s %d\n", id, seq);

        // Hacemos la comparación para el envío de alertas
        float valorf = atof(valor);
        printf("Valor a comparar: %.2f\n", valorf);
        /* >>> SIGUIENTE PASO: printf("Nivel: %s\n", evaluar(u, valorf)); */
        send(fd, resp, strlen(resp), 0);







    }


}

int main(void) {

    /* TODO A.3 */

    /* ===== ANDAMIO TEMPORAL (solo para probar A.1 y A.2) =====
     * Acepta UNA conexion, llama a handle_sonda y termina.
     * En A.3 borras todo este bloque y lo escribes tu, con fork. */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 10);
    printf("[andamio] esperando conexion en puerto %d...\n", PORT);
    int client_fd = accept(server_fd, NULL, NULL);
    handle_sonda(client_fd);
    close(client_fd);
    close(server_fd);
    /* ===== FIN ANDAMIO ===== */

    return 0;
}
