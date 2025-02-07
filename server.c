#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    int server_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    // Allow socket reuse
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }

    // Configure server address
    struct sockaddr_in serv_addr = { .sin_family = AF_INET,
                                     .sin_port = htons(4221),
                                     .sin_addr = { htonl(INADDR_ANY) }};

    // Bind to port
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");

    // Accept a single connection
    int conn = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    if (conn < 0) {
        printf("Accept failed: %s\n", strerror(errno));
        return 1;
    }
    printf("Client connected\n");

    // Read request
    char buff[1024] = {0};
    read(conn, buff, sizeof(buff));

    // Parse request
    strtok(buff, " ");
    char *path = strtok(NULL, " ");

    if (path == NULL) {
        char response[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(conn, response, strlen(response), 0);
    }

    else if (strncmp(path, "/user-agent", 11) == 0) {
        char *line = NULL;
        while ((line = strtok(NULL, "\r\n")) != NULL) {
            if (strncmp(line, "User-Agent: ", 12) == 0) {
                char *user_agent = line + 12;  // Skip "User-Agent: "
                
                const char *format = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s";
                
                char response[1024];
                snprintf(response, sizeof(response), format, strlen(user_agent), user_agent);
                send(conn, response, strlen(response), 0);
                break;
            }
        }
    }

    else if (strncmp(path, "/echo/", 6) == 0) {
        size_t contentLength = strlen(path) - 6;
        char *content = path + 6;

        const char *format = "HTTP/1.1 200 OK\r\nContent-Type: "
                             "text/plain\r\nContent-Length: %zu\r\n\r\n%s";

        char response[1024];
        snprintf(response, sizeof(response), format, contentLength, content);
        send(conn, response, strlen(response), 0);
    }

    else if (strcmp(path, "/") == 0) {
        char response[] = "HTTP/1.1 200 OK\r\n\r\n";
        send(conn, response, strlen(response), 0);
    }

    else {
        char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(conn, response, strlen(response), 0);
    }

    close(conn);
    close(server_fd);
    return 0;
}
