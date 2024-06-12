CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall

all: aesdsocket

aesdsocket: server.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f aesdsocket