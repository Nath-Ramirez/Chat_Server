#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h> 
#include <signal.h> 
#include <netdb.h> 

#define PORT 8080
#define BUFFER_SIZE 1024
#define RECONNECT_DELAY 5 
#define SERVER_IP "server" 

volatile sig_atomic_t keep_running = 1; 
volatile int connected = 0; 

int client_socket = -1; 
char username[50];


void handle_sigint(int sig) {
    printf("\nSeñal de interrupción recibida. Cerrando cliente...\n");
    keep_running = 0;
    connected = 0;
    if (client_socket != -1) {
        close(client_socket);
        client_socket = -1; 
    }
}

// Recibir mensajes del servidor
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    
    while (keep_running && connected) {
        pthread_testcancel(); 

        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); 

        if (!keep_running) break; 

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            
            printf("\r                                                "); 
            printf("\r[+] %s", buffer); 
            if (connected) { 
                printf("Escribe tu mensaje: "); 
            }
            fflush(stdout); 
        } else {
            
            if (keep_running) { 
                if (bytes_received == 0) {
                    printf("\rEl servidor cerró la conexión.\n");
                } else {
                    perror("\rError de recepción"); 
                }
                printf("Hilo terminado debido a desconexión.\n");
            }
            connected = 0;
        }
    }
    printf("Hilo receptor finalizando.\n"); 
    return NULL; 
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t recv_thread = 0; 

    
    signal(SIGINT, handle_sigint);

   
    printf("Ingrese su nombre de usuario: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        fprintf(stderr, "Error leyendo nombre de usuario.\n");
        return 1;
    }
    username[strcspn(username, "\n")] = 0; 

    const char *server_ip = SERVER_IP;
    if (argc > 1) {
        server_ip = argv[1]; 
    }
    printf("Usando IP del servidor: %s\n", server_ip);


   
    while (keep_running) {
        connected = 0; 

        printf("Creando socket...\n");
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("Error al crear el socket");
            printf("Reintentando en %d segundos...\n", RECONNECT_DELAY);
            sleep(RECONNECT_DELAY); 
            continue; 
        }

        
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", PORT);

        int status;
       
        if ((status = getaddrinfo(server_ip, port_str, &hints, &res)) != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            close(client_socket);
            client_socket = -1; 
            printf("Reintentando en %d segundos...\n", RECONNECT_DELAY); 
            sleep(RECONNECT_DELAY);
            continue;
        }

        memcpy(&server_addr, res->ai_addr, sizeof(struct sockaddr_in));
        freeaddrinfo(res); 

        printf("Intentando conectar a %s:%d...\n", server_ip, PORT);

        
        int connect_attempts = 0;
       
        while (keep_running && connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            connect_attempts++;
            perror("Error al conectar");
            printf("Reintentando conexión en %d segundos (Intento %d)...\n", RECONNECT_DELAY, connect_attempts); // Mensaje mejorado
            close(client_socket); 
            sleep(RECONNECT_DELAY);

            
            if (!keep_running) break; 
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket == -1) {
                perror("Error al re-crear el socket para reconexión");
                printf("Reintentando en %d segundos...\n", RECONNECT_DELAY); 
                sleep(RECONNECT_DELAY);
                
            }
        }

        if (!keep_running) { 
            if (client_socket != -1) close(client_socket);
            client_socket = -1;
            break; 
        }

        
        printf("Conectado al servidor.\n");
        connected = 1; 

        
        printf("Enviando nombre de usuario: %s\n", username);
        if (send(client_socket, username, strlen(username), 0) < 0) {
            if (keep_running) perror("Error al enviar el nombre de usuario"); 
            connected = 0; 
        } else {
            
            if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
                perror("Error al crear el hilo receptor");
                connected = 0; 
            } else {
                printf("Listo para chatear.\n");
            }
        }


        
        while (keep_running && connected) {
            printf("Escribe tu mensaje: ");
            fflush(stdout); 

            if (fgets(message, sizeof(message), stdin) == NULL) {
                if (feof(stdin)) {
                    printf("\nFin de la entrada (EOF). Cerrando...\n");
                } else {
                    perror("\nError leyendo stdin");
                }
                keep_running = 0; 
                connected = 0; 
                break; 
            }

            if (!keep_running || !connected) break; 

            if (strlen(message) > 1) { 
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    if (keep_running) perror("\nError al enviar mensaje"); 
                    connected = 0; 
                    break; 
                }
            }
        } 

        if (recv_thread != 0) { 
            printf("Esperando a que el hilo receptor finalice...\n");
            void *thread_result;
            int join_status = pthread_join(recv_thread, &thread_result); 
            if (join_status != 0) {
                fprintf(stderr, "Error al esperar al hilo receptor: %s\n", strerror(join_status));
            } else {
                
                if (thread_result == PTHREAD_CANCELED) {
                    printf("Hilo receptor cancelado o terminó exitosamente.\n"); 
                } else {
                    printf("Hilo receptor terminó normalmente.\n");
                }
            }
            recv_thread = 0; 
        }


        
        if (client_socket != -1) {
            printf("Cerrando socket %d\n", client_socket);
            close(client_socket);
            client_socket = -1; 
        }

        if (keep_running) {
            printf("Conexión perdida. Intentando reconectar en %d segundos...\n", RECONNECT_DELAY); 
        }

    } 

    printf("Cliente finalizado.\n");
    return 0;
}
