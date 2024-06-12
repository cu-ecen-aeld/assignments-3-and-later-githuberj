#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#define PORT 9000
#define MAX_PACKET_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

int server_fd;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        close(server_fd);
        remove(DATA_FILE);
        closelog();
        exit(0);
    }
}

void send_data_to_client(int client_socket) {
    FILE *fp = fopen(DATA_FILE, "r");
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        send(client_socket, buffer, strlen(buffer), 0);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    // Set up signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Open syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket");
        closelog();
        return -1;
    }

    // Bind socket to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket");
        closelog();
        return -1;
    }

   
    // Daemonize
    bool daemonize = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemonize = true;
    }
    if (daemonize) {
        pid_t pid = fork();
        if (pid == -1) {
            syslog(LOG_ERR, "Failed to fork");
            closelog();
            return -1;
        }
        if (pid != 0) {
            // Parent process
            exit(EXIT_SUCCESS);
        }
        umask(0);
    }

    // Listen for connections
    if (listen(server_fd, 1) == -1) {
        syslog(LOG_ERR, "Failed to listen for connections");
        closelog();
        return -1;
    }

    while (true) {
        int new_socket = accept(server_fd, NULL, NULL);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        char buffer[1024];
        ssize_t bytes_received;
        FILE *fp = fopen(DATA_FILE, "a+");

        while ((bytes_received = recv(new_socket, buffer, sizeof(buffer), 0)) > 0) {
            buffer[bytes_received] = '\0';
            fprintf(fp, "%s", buffer);

            char *newline = strchr(buffer, '\n');
            if (newline != NULL) {
                fflush(fp);
                send_data_to_client(new_socket);
                memset(buffer, 0, sizeof(buffer));
            }
        }

        close(new_socket);
        fclose(fp);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    // Clean up and exit
    closelog();
    close(server_fd);
    return 0;
}