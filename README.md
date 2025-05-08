# Sistema de Chat Simple con Docker y C

## Introducción

Este proyecto implementa un sistema de chat simple son varios clientes utilizando el lenguaje de programación C y Docker. El objetivo principal es demostrar una comunicación básica en red entre un servidor central y múltiples clientes, permitiendo el envío y recepción de mensajes en tiempo real. Se utiliza sockets de red para la comunicación y se gestiona la concurrencia en el servidor mediante hilos (pthreads). El proyecto se ejecuta mediante Docker.

## Desarrollo

El sistema de chat se compone de dos aplicaciones principales: un servidor (`server.c`) y un cliente (`client.c`).

### Servidor (`server.c`)

El servidor es la pieza central del sistema. Su funcionamiento se basa en los siguientes pasos:

1.  **Creación y configuración del socket:** Se crea un socket de tipo `AF_INET` y `SOCK_STREAM` (TCP).
2.  **Enlace (bind):** El socket se enlaza a una dirección IP (se utiliza `INADDR_ANY` para escuchar en todas las interfaces) y un puerto específico (definido como `8080`).
3.  **Escucha (listen):** El servidor se pone en modo de escucha, esperando conexiones entrantes de los clientes. Se define un máximo de clientes (`MAX_CLIENTS = 10`).
4.  **Aceptación de conexiones (accept):** En un bucle infinito, el servidor acepta las conexiones entrantes de los clientes mediante la función `accept()`. Por cada conexión aceptada, se crea un nuevo socket (`client_socket`) para la comunicación con ese cliente específico.
5.  **Manejo de clientes con hilos:** Para permitir la comunicación simultánea con múltiples clientes, cada conexión aceptada se gestiona en un hilo separado (`pthread_create()`). La función `handle_client()` es la responsable de la comunicación individual con cada cliente.
6.  **Recepción de nombre de usuario:** Al conectar, el cliente envía su nombre de usuario al servidor.
7.  **Broadcast de mensajes:** Cuando un cliente envía un mensaje, el servidor lo recibe y lo reenvía (broadcast) a todos los demás clientes conectados, excluyendo al emisor.
8.  **Registro de actividad:** La actividad del servidor (conexiones, desconexiones, mensajes broadcast) se registra en un archivo de logs. Se utilizan mutex (`pthread_mutex_t`) para proteger el acceso a la lista de clientes y al archivo de logs, evitando condiciones de carrera.
9.  **Desconexión de clientes:** Cuando un cliente se desconecta (cierra la conexión o se detecta un error), el servidor lo detecta, informa a los demás clientes y libera los recursos asociados.

### Cliente (`client.c`)

El cliente es la aplicación que los usuarios ejecutan para conectarse al servidor de chat. Su funcionamiento incluye:

1.  **Solicitud de nombre de usuario:** Al iniciar, el cliente solicita al usuario que ingrese un nombre de usuario.
2.  **Creación del socket:** Se crea un socket de tipo `AF_INET` y `SOCK_STREAM`.
3.  **Resolución de la dirección del servidor:** Se utiliza `getaddrinfo()` para obtener la dirección IP del servidor a partir de su nombre (`SERVER_IP`, definido como "server" y el puerto `PORT = 8080`).
4.  **Conexión al servidor (connect):** El cliente intenta conectarse al servidor utilizando la dirección IP y el puerto obtenidos. Se implementa una lógica de reintento de conexión con un retardo (`RECONNECT_DELAY = 5` segundos) en caso de fallo inicial o pérdida de conexión.
5.  **Envío del nombre de usuario:** Una vez conectado, el cliente envía su nombre de usuario al servidor.
6.  **Recepción de mensajes (hilo separado):** Se crea un hilo (`pthread_create()`) que ejecuta la función `receive_messages()`. Este hilo se encarga de recibir continuamente los mensajes enviados por el servidor y los muestra en la consola del cliente. Se utiliza mecanismos de cancelación de hilo para una terminación limpia.
7.  **Envío de mensajes del usuario:** El hilo principal del cliente lee la entrada del usuario desde la consola y envía los mensajes al servidor.
8.  **Manejo de desconexión:** Si el servidor cierra la conexión o se detecta un error de recepción, el hilo de recepción finaliza y el cliente intenta reconectarse periódicamente.

### Docker (`Dockerfile`)

El `Dockerfile` nos ayuda a comunicarnos con docker para crear las imágenes de los archivos `client.c` y `server.c`. Estas imágenes posteriormente nos servirán para crear las instancias (contenedores) de el server y los múltiples cliente.

### Estructura Docker

Nuestra estructua en Docker se basa en:

- Dos imágenes: `chat_server` para el server y `chat_client` para el cliente
- Área para crear los contenedores: instancias de las imágenes de el server y el ciente
- Red: `chat_network` a la cual se conecta el servidor y los clientes
- Volumen: `chat-logs` para que el archivo logs.txt tenga consistencia de datos y pueda observarse por fuera del entorno de Docker.

### Aspectos logrados y no logrados
Aspectos logrados
- Implementación de un chat con un servidor y varios clientes con C y Docker, usando sockets.
- Gestión de la concurrencia en el servidor mediante hilos (pthreads) para manejar múltiples clientes simultáneamente.
- Funcionalidad de broadcast de mensajes: los mensajes enviados por un cliente son recibidos por todos los demás clientes conectados.
- Implementación de un cliente en C que puede conectarse al servidor, enviar su nombre de usuario y participar en el chat enviando y recibiendo mensajes.
- Manejo básico de desconexiones tanto en el servidor como en el cliente.
- Registro de la actividad del servidor en un archivo de logs.
- Contenerización del servidor utilizando Docker para facilitar su despliegue y ejecución.
- Implementación de reintento de conexión en el cliente.

### Conclusiones
Se ha desarrollado un sistema de chat funcional que demuestra los conceptos básicos de la comunicación en red utilizando sockets y la gestión de la concurrencia con hilos en C. La utilización de Docker facilita la ejecución del servidor. El proyecto sienta una base sólida para futuras mejoras, como la adición de funcionalidades más avanzadas y una interfaz de usuario más intuitiva.

## Referencias
- Documentación de las funciones de socket de Linux (man socket, man bind, man listen, man accept, man connect, man send, man recv, man close).
- Documentación de la biblioteca de hilos POSIX (man pthreads).
- Documentación de Docker (https://docs.docker.com/).
- Gemini
- Ejemplos y tutoriales de programación de chat con sockets en C.
