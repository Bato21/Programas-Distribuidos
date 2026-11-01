#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     7070

typedef struct {
    int  ts;
    char payload[64];
} Msg;

/* Fragmento del cliente */
void client_send(int fd, int *clock, const char *text) {
    Msg msg;
    /* TODO: incrementar reloj, adjuntar timestamp, enviar */
    (*clock)++;
    msg.ts = *clock;
    snprintf(msg.payload, sizeof(msg.payload), "%s", text);
    send(fd, &msg, sizeof(msg), 0);
}