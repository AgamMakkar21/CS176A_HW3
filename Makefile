CC = gcc
CFLAGS = -Wall

all: PingClient

PingClient: PingClient.c
	$(CC) $(CFLAGS) -o PingClient PingClient.c

clean:
	rm -f PingClient