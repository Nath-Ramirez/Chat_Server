#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h> // Para perror
#include <signal.h> // Para manejo de señales
#include <netdb.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define RECONNECT_DELAY 5 // Segundos de espera antes de reintentar
// Define la IP del servidor aquí o pásala como argumento
#define SERVER_IP "server" // IP DEL CONTENEDOR DEL SERVER DENTRO DE LA RED DE DOCKER

volatile sig_atomic_t keep_running = 1; // Para manejar cierre limpio con Ctrl+C

int client_socket = -1; // Inicializar como inválido
char username[50];

// Función para manejar Ctrl+C
void handle_sigint(int sig) {
    keep_running = 0;
    printf("\nSeñal de interrupción recibida. Cerrando cliente...\n");
    // Podríamos intentar cerrar el socket aquí, pero es mejor en el main loop
}

// Recibir mensajes del servidor
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Habilitar la cancelación del hilo de forma segura
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL); // Esperar a puntos de cancelación

    while (keep_running) {
        pthread_testcancel(); // Punto de cancelación explícito

        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // Dejar espacio para '\0'

        if (!keep_running) break; // Salir si se recibió SIGINT

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            // Imprimir mensaje y volver a mostrar el prompt de entrada
            printf("\r                          ");
            printf("\r[+] %s", buffer); // \r para volver al inicio de línea
            printf("Escribe tu mensaje: "); // Volver a mostrar prompt
            fflush(stdout); // Asegurar que se muestre inmediatamente
        } else {
            // 0: Conexión cerrada por el servidor. -1: Error.
            if (bytes_received == 0) {
                printf("\rEl servidor cerró la conexión.\n");
            } else {
                // Solo mostrar error si keep_running todavía es true (no es un cierre intencional)
                 if (keep_running) {
                    perror("\rError");
                 }
            }
            printf("Hilo terminado debido a desconexión.\n");
            // No cerramos el socket aquí, main lo hará.
            // Forzar la salida del bucle de envío en main podría hacerse con una señal o flag,
            // pero main también detectará el error en send().
            return NULL; // Terminar el hilo
        }
    }
    printf("Hilo finalizando limpiamente.\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];
    pthread_t recv_thread = 0; // Inicializar a 0 o PTHREAD_NULL

    // Configurar manejo de Ctrl+C
    signal(SIGINT, handle_sigint);

    // Obtener el nombre de usuario una sola vez al inicio
    printf("Ingrese su nombre de usuario: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        fprintf(stderr, "Error leyendo nombre de usuario.\n");
        return 1;
    }
    username[strcspn(username, "\n")] = 0; // Eliminar salto de línea

    const char *server_ip = SERVER_IP;
    if (argc > 1) {
        server_ip = argv[1]; // Permitir pasar IP como argumento (opcional)
    }
    printf("Usando IP del servidor: %s\n", server_ip);


    // ----- Bucle principal de conexión/reconexión -----
    while (keep_running) {
        printf("Creando socket...\n");
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("Error al crear el socket");
            sleep(RECONNECT_DELAY); // Esperar antes de reintentar
            continue; // Volver al inicio del bucle while
        }

        // Configurar dirección del servidor
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
            sleep(RECONNECT_DELAY);
            continue;
        }

        memcpy(&server_addr, res->ai_addr, sizeof(struct sockaddr_in));
        freeaddrinfo(res); // Liberar memoria

        printf("Intentando conectar a %s:%d...\n", server_ip, PORT);

        // Intentar conectar
        while (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            if (!keep_running) break; // Salir si se recibió SIGINT
            perror("Error al conectar");
            printf("Reintentando conexión en %d segundos...\n", RECONNECT_DELAY);
            close(client_socket); // Cerrar el socket fallido
            sleep(RECONNECT_DELAY);

            // Necesitamos crear un nuevo socket para el siguiente intento
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
             if (client_socket == -1) {
                perror("Error al re-crear el socket");
                sleep(RECONNECT_DELAY);
                // Volver al inicio del bucle connect? O del while principal?
                // Mejor continuar el bucle connect, pero si falla mucho, el while externo ayuda.
             }
             if (client_socket == -1) continue; // Si falló la re-creación, reintenta desde cero
        }

        if (!keep_running) { // Si salimos del bucle connect por SIGINT
            if (client_socket != -1) close(client_socket);
            break; // Salir del bucle while principal
        }

        printf("Conectado al servidor.\n");

        // Enviar nombre de usuario (IMPORTANTE: hacer esto después de CADA conexión)
        printf("Enviando nombre de usuario: %s\n", username);
        if (send(client_socket, username, strlen(username), 0) < 0) {
            if (!keep_running) break; // Checkear SIGINT
            perror("Error al enviar el nombre de usuario");
            close(client_socket); // Cerrar y reintentar conexión
            client_socket = -1;
            sleep(RECONNECT_DELAY);
            continue; // Volver al inicio del bucle while principal
        }

        // Crear e iniciar el hilo para recibir mensajes
        if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
            perror("Error al crear el hilo");
            close(client_socket); // Cerrar y reintentar conexión
            client_socket = -1;
            sleep(RECONNECT_DELAY);
            continue; // Volver al inicio del bucle while principal
        }

        printf("Listo para chatear.\n");

        // ----- Bucle de envío de mensajes -----
        int send_error = 0;
        while (keep_running && !send_error) {
            printf("Escribe tu mensaje: ");
            fflush(stdout); // Asegurar que el prompt se muestre

            // Usar select o poll aquí sería más robusto para manejar stdin y socket
            // Pero para simplificar, usamos fgets bloqueante
            if (fgets(message, sizeof(message), stdin) == NULL) {
                if (feof(stdin)) {
                    printf("\nFin de la entrada (EOF). Cerrando...\n");
                    keep_running = 0; // Señalizar cierre limpio
                } else {
                    perror("\nError leyendo stdin");
                    keep_running = 0; // Salir en caso de error
                }
                break; // Salir del bucle de envío
            }

            if (!keep_running) break; // Salir si SIGINT durante fgets

            if (strlen(message) > 1) { // Evitar enviar solo Enter
                if (send(client_socket, message, strlen(message), 0) < 0) {
                    if (!keep_running) break; // Checkear SIGINT
                    perror("\nError al enviar mensaje");
                    send_error = 1; // Marcar error para salir del bucle
                    // No cerramos el socket aquí, se hará después de limpiar el hilo
                }
            }
        } // Fin del bucle de envío

        // ----- Limpieza antes de reintentar conexión o salir -----

        // 1. Señalizar al hilo receptor que debe terminar (si no lo ha hecho ya)
        //    y esperar a que termine.
        if (recv_thread != 0) { // Asegurarse que el hilo fue creado
            int cancel_status = pthread_cancel(recv_thread); // Solicitar cancelación
             if (cancel_status != 0) {
                 fprintf(stderr, "Error al cancelar el hilo: %s\n", strerror(cancel_status));
             }
             void *thread_result;
             int join_status = pthread_join(recv_thread, &thread_result); // Esperar a que termine
             if (join_status != 0) {
                  fprintf(stderr, "Error al esperar al hilo: %s\n", strerror(join_status));
             } else {
                  if (thread_result == PTHREAD_CANCELED) {
                      printf(" Hilo cancelado exitosamente.\n");
                  } else {
                      printf(" Hilo terminó normalmente.\n");
                  }
             }
             recv_thread = 0; // Marcar como no válido
        }


        // 2. Cerrar el socket (si aún es válido)
        if (client_socket != -1) {
            printf("Cerrando socket %d\n", client_socket);
            close(client_socket);
            client_socket = -1;
        }

        if (keep_running) {
            printf("Esperando %d segundos antes de intentar reconectar...\n", RECONNECT_DELAY);
            sleep(RECONNECT_DELAY);
        }

    } // ----- Fin del bucle principal de conexión/reconexión -----

    printf("Cliente finalizado.\n");
    return 0;
}