FROM debian:latest

# Instala dependencias necesarias
RUN apt-get update && apt-get install -y gcc make iproute2 net-tools iputils-ping

# Copia los archivos del servidor al contenedor
WORKDIR /app
COPY server.c /app/server.c

# Compila el servidor
RUN gcc /app/server.c -o /app/server -lpthread

# Expone el puerto 8080
EXPOSE 8080

# Comando para ejecutar el servidor
CMD ["/app/server"]