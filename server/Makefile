#CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -pthread 
LDFLAGS = -Wl,--hash-style=gnu
all: aesdsocket

aesdsocket: server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f aesdsocket