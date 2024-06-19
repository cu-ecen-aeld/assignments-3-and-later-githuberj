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
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "queue.h"

#define PORT 9000
#define MAX_PACKET_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

#define syslog(priority, ...) \
    printf(__VA_ARGS__);      \
    printf("\n");

volatile int server_fd;
volatile int done = 0;

struct node
{
    pthread_t *thread;
    struct client_data *data;
    // This macro does the magic to point to other nodes
    TAILQ_ENTRY(node)
    nodes;
};

void handle_signal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        close(server_fd);
        done = 1;
        // exit(0);
    }
}

void send_data_to_client(int client_socket)
{
    FILE *fp = fopen(DATA_FILE, "r");
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        send(client_socket, buffer, strlen(buffer), 0);
    }
    fclose(fp);
}

void read_from_socket_to_file(int client_socket)
{
    char buffer[1024];
    ssize_t bytes_received;
    FILE *fp = fopen(DATA_FILE, "a");

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        fprintf(fp, "%s", buffer);

        char *newline = strchr(buffer, '\n');
        if (newline != NULL)
        {
            fflush(fp);
            break;
        }
    }
    fclose(fp);
}

struct client_data
{
    int client_socket;
    char client_ip[INET_ADDRSTRLEN];
    bool *done;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *write_timestamp_to_file(void *arg)
{
    int i = 0;
    while (!done)
    {
        i++;
        sleep(1);
        if (i % 10 == 0)
        {
            pthread_mutex_lock(&mutex);
            FILE *fp = fopen(DATA_FILE, "a");
            time_t t;
            struct tm *tmp;
            char MY_TIME[50];
            time(&t);
            char strRFC2822[] = "timestamp:%a, %d %b %Y %T %z\n";

            tmp = localtime(&t);
            // using strftime to display time
            strftime(MY_TIME, sizeof(MY_TIME), strRFC2822, tmp);
            printf("Formatted date & time : %s\n", MY_TIME);

            fprintf(fp, "Timestamp: %s \n", MY_TIME);
            fclose(fp);
            pthread_mutex_unlock(&mutex);
        }
    }

    return NULL;
}

void *execute_commands(void *arg)
{
    // Code to execute commands goes here

    struct client_data *data = (struct client_data *)arg;

    pthread_mutex_lock(&mutex);
    read_from_socket_to_file(data->client_socket);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    send_data_to_client(data->client_socket);
    pthread_mutex_unlock(&mutex);

    close(data->client_socket);
    syslog(LOG_INFO, "Closed connection from %s", data->client_ip);

    *data->done = 1;
    return NULL;
}

int main(int argc, char *argv[])
{
    TAILQ_HEAD(head_s, node)
    head;
    // Initialize the head before use
    TAILQ_INIT(&head);
    // Set up signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Open syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        syslog(LOG_ERR, "Failed to create socket");
        closelog();
        return -1;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    // Bind socket to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        syslog(LOG_ERR, "Failed to bind socket");
        closelog();
        return -1;
    }

    // Daemonize
    bool daemonize = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        daemonize = true;
    }
    if (daemonize)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            syslog(LOG_ERR, "Failed to fork");
            closelog();
            return -1;
        }
        if (pid != 0)
        {
            // Parent process
            exit(EXIT_SUCCESS);
        }
        umask(0);
    }

    // Listen for connections
    if (listen(server_fd, 1) == -1)
    {
        syslog(LOG_ERR, "Failed to listen for connections");
        closelog();
        return -1;
    }

    // Start a separate thread to write timestamp to file every 10 seconds
    pthread_t timestamp_thread;
    bool time_thread_started = false;

    while (true)
    {
        int new_socket = accept(server_fd, NULL, NULL);

        if (time_thread_started == false) {
            time_thread_started = true;
            pthread_create(&timestamp_thread, NULL, write_timestamp_to_file, NULL);
        }

        if (new_socket == -1)
        {
            break;
        }

        bool *done = malloc(sizeof(bool));
        *done = false;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
        struct client_data *data = (struct client_data *)malloc(sizeof(struct client_data));
        data->client_socket = new_socket;
        strcpy(data->client_ip, client_ip);
        data->done = done;
        pthread_create(thread, NULL, execute_commands, (void *)data);

        // pthread_join(*thread, NULL);
        struct node *n = malloc(sizeof(struct node));
        n->thread = thread;
        n->data = data;
        TAILQ_INSERT_TAIL(&head, n, nodes);
        n = NULL;

        n = TAILQ_FIRST(&head);

        while (n != NULL)
        {
            if (*(n->data->done) == true)
            {
                pthread_join(*(n->thread), NULL);
                TAILQ_REMOVE(&head, n, nodes);
                free(n->thread);
                free(n->data->done);
                free(n->data);
                free(n);
            }
            n = TAILQ_NEXT(n, nodes);
        }
    }

    printf("cleaning up\n");

    pthread_join(timestamp_thread, NULL);
    struct node *n = NULL;

    while (!TAILQ_EMPTY(&head))
    {
        n = TAILQ_FIRST(&head);
        TAILQ_REMOVE(&head, n, nodes);
        pthread_join(*(n->thread), NULL);
        free(n->thread);
        free(n->data->done);
        free(n->data);
        free(n);
        n = NULL;
    }

    // Clean up and exit
    closelog();
    close(server_fd);
    remove(DATA_FILE);
    return 0;
}