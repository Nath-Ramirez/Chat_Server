#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Estructura para guardar la info de cada cliente
typedef struct {
    int socket;
    char username[50];
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t archivo_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *archivo;

// Enviar mensaje a todos menos al que envió
void broadcast_message(char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    printf("[Broadcast] %s", message);
    pthread_mutex_lock(&archivo_mutex);
    fprintf(archivo, "[Broadcast] %s", message);
    fflush(archivo);
    pthread_mutex_unlock(&archivo_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0 && clients[i].socket != sender_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Manejo de cada cliente en un hilo separado
void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    char formatted_message[BUFFER_SIZE + 50];
    char username[50];

    printf("Cliente conectado (socket %d)\n", client_socket);
    pthread_mutex_lock(&archivo_mutex);
    fprintf(archivo, "Cliente conectado (socket %d)\n", client_socket);
    fflush(archivo);
    pthread_mutex_unlock(&archivo_mutex);

    //Recibir nombre de usuario
    int bytes = recv(client_socket, username, sizeof(username) - 1, 0);
    if (bytes <= 0) {
        printf("Error: No se recibió el nombre de usuario.\n");
        close(client_socket);
        pthread_exit(NULL);
    }
    username[bytes] = '\0';
    printf("Usuario conectado: %s\n", username);
    pthread_mutex_lock(&archivo_mutex);
    fprintf(archivo, "Usuario conectado: %s\n", username);
    fflush(archivo);
    pthread_mutex_unlock(&archivo_mutex);

    // ➕ Agregar cliente a la lista
    pthread_mutex_lock(&clients_mutex);
    int added = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == 0) {
            clients[i].socket = client_socket;
            strcpy(clients[i].username, username);
            added = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!added) {
        printf("No hay espacio para más clientes.\n");
        pthread_mutex_lock(&archivo_mutex);
        fprintf(archivo, "No hay espacio para más clientes.\n");
        fflush(archivo);
        pthread_mutex_unlock(&archivo_mutex);
        close(client_socket);
        pthread_exit(NULL);
    }

    // Anunciar nueva conexión
    snprintf(formatted_message, sizeof(formatted_message), "%s se ha unido al chat\n", username);
    pthread_mutex_lock(&archivo_mutex);
    fprintf(archivo, "%s se ha unido al chat\n", username);
    fflush(archivo);
    pthread_mutex_unlock(&archivo_mutex);
    broadcast_message(formatted_message, client_socket);

    // Loop de recepción de mensajes
    while (1) {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("%s se ha desconectado (socket %d)\n", username, client_socket);
            pthread_mutex_lock(&archivo_mutex);
            fprintf(archivo, "%s se ha desconectado (socket %d)\n", username, client_socket);
            fflush(archivo);
            pthread_mutex_unlock(&archivo_mutex);
            break;
        }

        buffer[bytes_received] = '\0';
        printf("[%s]: %s", username, buffer);

        snprintf(formatted_message, sizeof(formatted_message), "%s: %s", username, buffer);
        broadcast_message(formatted_message, client_socket);
    }

    // Eliminar cliente desconectado
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_socket) {
            clients[i].socket = 0;
            snprintf(formatted_message, sizeof(formatted_message), " %s ha salido del chat\n", clients[i].username);
            pthread_mutex_lock(&archivo_mutex);
            fprintf(archivo, " %s ha salido del chat\n", clients[i].username);
            fflush(archivo);
            pthread_mutex_unlock(&archivo_mutex);
            broadcast_message(formatted_message, client_socket);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    // Crear el archivo txt
    archivo = fopen("/app/logs.txt", "w");
    if(archivo == NULL){
        printf("Error al crear el archivo logs.txt\n");
        return 1;
    }
    // Crear socket del servidor
    printf("Creando socket del servidor...\n");
    pthread_mutex_lock(&archivo_mutex);
    fprintf(archivo, "Creando socket del servidor...\n");
    fflush(archivo);
    pthread_mutex_unlock(&archivo_mutex);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error al crear el socket del servidor");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Enlazar socket a la IP y puerto
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al hacer bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones entrantes
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Error al escuchar");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado en el puerto %d\n", PORT);
    pthread_mutex_lock(&archivo_mutex);
    fprintf(archivo, "Servidor iniciado en el puerto %d\n", PORT);
    fflush(archivo);
    pthread_mutex_unlock(&archivo_mutex);

    // Aceptar conexiones en loop
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket >= 0) {
            printf("Nueva conexión: IP %s, Puerto %d\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            pthread_mutex_lock(&archivo_mutex);
            fprintf(archivo, "Nueva conexión: IP %s, Puerto %d\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            fflush(archivo);
            pthread_mutex_unlock(&archivo_mutex);
            
            pthread_t tid;
            int *new_sock = malloc(sizeof(int));
            *new_sock = client_socket;

            pthread_create(&tid, NULL, handle_client, new_sock);
            pthread_detach(tid);
        }
    }

    fclose(archivo);
    close(server_socket);
    return 0;
}
