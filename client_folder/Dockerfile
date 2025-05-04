FROM debian:latest

# Instala gcc y make
RUN apt-get update && apt-get install -y gcc make iproute2 net-tools iputils-ping libnet-dev

# Copia los archivos del cliente
WORKDIR /app
COPY client.c /app/client.c

# Compila el cliente
RUN gcc client.c -o client -lpthread

# Comando para ejecutar el cliente
CMD ["./client"]