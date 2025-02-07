#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = { .sin_family = AF_INET,
                                     .sin_port = htons(4221),
                                     .sin_addr = { htonl(INADDR_ANY) },
                                   };

    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }
        printf("Client connected\n");

        char *response = (char*)calloc(1024, sizeof(char));
        int bytes_received = recv(client_fd, response, 1024, 0);
        if (bytes_received <= 0) {
            printf("There was an error or a client disconnect \n");
            close(client_fd);
            free(response);
            continue;
        }
        response[bytes_received] = '\0';

        char *request_line = strtok(response, "\r\n");
        if (request_line != NULL) {
            char *method = strtok(request_line, " ");   // "GET"
            char *path = strtok(NULL, " ");             // "/home"
            char *version = strtok(NULL, " ");          // "HTTP/1.1"

            char reply_send[1024];

            if (path != NULL) {
                if (strcmp(path, "/") == 0) {
                    snprintf(reply_send, sizeof(reply_send), "HTTP/1.1 200 OK\r\n\r\n");
                }
                else if (strncmp(path, "/echo/", 6) == 0) { // Check for /echo/{str}
                    char *echo_response = path + 6;  // Extract the echo string
                    snprintf(reply_send, sizeof(reply_send),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/plain\r\n"
                             "Content-Length: %zu\r\n\r\n"
                             "%s",
                             strlen(echo_response), echo_response);
                }
                else {
                    snprintf(reply_send, sizeof(reply_send), 
                             "HTTP/1.1 400 Bad Request\r\n"
                             "Content-Length: 0\r\n\r\n");
                }
            } else {
                snprintf(reply_send, sizeof(reply_send), 
                         "HTTP/1.1 400 Bad Request\r\n"
                         "Content-Length: 0\r\n\r\n");
            }

            send(client_fd, reply_send, strlen(reply_send), 0);
        }

        close(client_fd);
        free(response);
    }

    close(server_fd);
    return 0;
}
