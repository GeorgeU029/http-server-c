#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    char *not_found_404 = "HTTP/1.1 404 NOT FOUND \r\n\r\n";
    printf("Logs from your program will appear here!\n");
    char mesg_boy[BUFSIZ];
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
    if (listen(server_fd, 10) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");

    signal(SIGCHLD, SIG_IGN);

    while(1) {
        while (waitpid(-1, NULL, WNOHANG) > 0);

        // Accept a single connection
        int conn = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (conn < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        if (!fork()) {  // Child process
            close(server_fd);
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

            else if (strncmp(path, "/files", 6) == 0 && argc >= 3 && argv[2] != NULL) {
                char *directory = argv[2]; // Get the directory from --directory argument

                // Extract filename from the path
                strtok(path, "/");  // "/files"
                char *filename = strtok(NULL, "\0");

                if (filename == NULL) {
                    write(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
                    close(conn);
                    exit(0);
                }

                // Construct full file path
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);

                // Check if file exists
                if (access(filepath, F_OK) != 0) {
                    write(conn, not_found_404, strlen(not_found_404));
                    close(conn);
                    exit(0);
                }

                // Open file for reading
                FILE *fptr = fopen(filepath, "rb");
                if (!fptr) {
                    perror("Error opening file");
                    write(conn, not_found_404, strlen(not_found_404));
                    close(conn);
                    exit(0);
                }

                // Determine file size
                fseek(fptr, 0, SEEK_END);
                size_t size = ftell(fptr);
                fseek(fptr, 0, SEEK_SET);

                // Handle empty file
                if (size == 0) {
                    write(conn, "HTTP/1.1 204 No Content\r\n\r\n", 26);
                    fclose(fptr);
                    close(conn);
                    exit(0);
                }

                // Send HTTP headers
                char header[256];
                snprintf(header, sizeof(header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/octet-stream\r\n"
                        "Content-Length: %zu\r\n\r\n", size);
                write(conn, header, strlen(header));

                // Stream the file in chunks
                char buffer[BUFSIZ];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, BUFSIZ, fptr)) > 0) {
                    write(conn, buffer, bytes_read);
                }

                // Cleanup
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

            close(conn);
            exit(0);
        }

        close(conn);
    }

    return 0;
}
