#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// This function sets up the server socket (socket creation, options, binding, and listening)
int create_server_socket() {
    int server_fd;
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        exit(1);
    }

    // Allow socket reuse
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        exit(1);
    }

    // Configure server address
    struct sockaddr_in serv_addr = { .sin_family = AF_INET,
                                     .sin_port = htons(4221),
                                     .sin_addr = { htonl(INADDR_ANY) }};

    // Bind to port
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        exit(1);
    }
    return server_fd;
}

// This function processes a single client connection. All of the original request handling code is preserved.
void process_client(int conn, char *directory, char *not_found_404) {
    printf("Client connected\n");

    // Read request
    char buff[1024] = {0};
    read(conn, buff, sizeof(buff));

    // Extract HTTP method and path
    char *method = strtok(buff, " ");
    char *path = strtok(NULL, " ");

    if (method == NULL || path == NULL) {
        char response[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(conn, response, strlen(response), 0);
    } 

    else if (strcmp(method, "GET") == 0) {
        if (strncmp(path, "/user-agent", 11) == 0) {
            char *line = NULL;
            while ((line = strtok(NULL, "\r\n")) != NULL) {
                if (strncmp(line, "User-Agent: ", 12) == 0) {
                    char *user_agent = line + 12;
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
            const char *format = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s";
            char response[1024];
            snprintf(response, sizeof(response), format, contentLength, content);
            send(conn, response, strlen(response), 0);
        }

        else if (strncmp(path, "/files", 6) == 0) {
            char path_copy[1024];
            strncpy(path_copy, path, sizeof(path_copy) - 1);
            path_copy[sizeof(path_copy) - 1] = '\0';

            strtok(path_copy, "/");
            char *filename = strtok(NULL, "\0");

            if (!filename) {
                write(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
                close(conn);
                exit(0);
            }

            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);

            if (access(filepath, F_OK) != 0) {
                write(conn, not_found_404, strlen(not_found_404));
                close(conn);
                exit(0);
            }

            FILE *fptr = fopen(filepath, "rb");
            if (!fptr) {
                perror("Error opening file");
                write(conn, not_found_404, strlen(not_found_404));
                close(conn);
                exit(0);
            }

            fseek(fptr, 0, SEEK_END);
            size_t size = ftell(fptr);
            fseek(fptr, 0, SEEK_SET);

            if (size == 0) {
                write(conn, "HTTP/1.1 204 No Content\r\n\r\n", 26);
                fclose(fptr);
                close(conn);
                exit(0);
            }

            char header[256];
            snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %zu\r\n\r\n", size);
            write(conn, header, strlen(header));

            char buffer[BUFSIZ];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, BUFSIZ, fptr)) > 0) {
                write(conn, buffer, bytes_read);
            }

            fclose(fptr);
            close(conn);
            exit(0);
        }

        else if (strcmp(path, "/") == 0) {
            char response[] = "HTTP/1.1 200 OK\r\n\r\n";
            send(conn, response, strlen(response), 0);
        }

        else {
            char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
            send(conn, response, strlen(response), 0);
        }
    }

    else if (strcmp(method, "POST") == 0) {
        char path_copy[1024];
        strncpy(path_copy, path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        strtok(path_copy, "/");
        char *filename = strtok(NULL, "\0");

        int content_length = 0;
        char *line = NULL;
        while ((line = strtok(NULL, "\r\n")) != NULL) {
            if (strncmp(line, "Content-Length:", 15) == 0) {
                content_length = atoi(line + 15);
                break;
            }
        }

        char *body = malloc(content_length + 1);
        ssize_t total_read = 0;
        while (total_read < content_length) {
            ssize_t bytes = read(conn, body + total_read, content_length - total_read);
            if (bytes <= 0) break;
            total_read += bytes;
        }
        body[content_length] = '\0';

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);
        FILE *file_pointer = fopen(filepath, "wb");
        fwrite(body, 1, content_length, file_pointer);
        fclose(file_pointer);
        free(body);

        char response[] = "HTTP/1.1 201 Created\r\n\r\n";
        send(conn, response, strlen(response), 0);
    }

    close(conn);
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    char *not_found_404 = "HTTP/1.1 404 NOT FOUND \r\n\r\n";
    printf("Logs from your program will appear here!\n");
    char mesg_boy[BUFSIZ];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char *directory = NULL;

    // Ensure directory is provided via --directory flag
    if (argc >= 3 && strcmp(argv[1], "--directory") == 0) {
        directory = argv[2];
    } else {
        printf("Error: --directory flag is required.\n");
        return 1;
    }

    int server_fd = create_server_socket();

    printf("Waiting for a client to connect...\n");

    signal(SIGCHLD, SIG_IGN);

    while (1) {
        while (waitpid(-1, NULL, WNOHANG) > 0);

        // Accept a single connection
        int conn = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (conn < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        if (!fork()) {  // Child process
            close(server_fd);
            process_client(conn, directory, not_found_404);
            exit(0);
        }
        close(conn);
    }
    return 0;
}
