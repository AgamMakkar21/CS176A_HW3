CC = gcc
CFLAGS = -Wall -O2

# Default target
all: PingClient

# Build the PingClient executable
PingClient: PingClient.c
	$(CC) $(CFLAGS) -o PingClient PingClient.c

# Clean up generated files
clean:
	rm -f PingClient