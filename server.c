#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#define PORT 8080

// File Handling (MIME type mapping)
struct { char *file_extension; char *mime_type; } mime_type_mappings[] = {
    {".html", "text/html"}, {".css", "text/css"}, {".js", "application/javascript"},
    {".jpg", "image/jpeg"}, {".png", "image/png"}, {".mp3", "audio/mpeg"}, 
    {".mp4", "video/mp4"}, {NULL, NULL}
};

void *handle_client_request(void *client_socket_argument) {
    int client_socket = (long)client_socket_argument;
    char request_buffer[1024] = {0}, request_method[8], request_path[256];
    
    // Receive and parse request
    if (recv(client_socket, request_buffer, sizeof(request_buffer) - 1, 0) > 0 && sscanf(request_buffer, "%s %s", request_method, request_path) == 2) {
        printf("Request: %s %s\n", request_method, request_path);
        
        char *requested_file_path = (strcmp(request_path, "/") == 0) ? "index.html" : request_path + 1; // Map root path to index.html
        int file_descriptor = open(requested_file_path, O_RDONLY);
        
        if (file_descriptor != -1) {
            struct stat file_status;
            fstat(file_descriptor, &file_status);
            
            // Content type
            char *response_mime_type = "application/octet-stream";
            char *file_extension = strrchr(requested_file_path, '.');
            for (int i = 0; file_extension && mime_type_mappings[i].file_extension; i++)
                if (strcmp(file_extension, mime_type_mappings[i].file_extension) == 0) response_mime_type = mime_type_mappings[i].mime_type;

            // Send headers 1st
            dprintf(client_socket, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", response_mime_type, file_status.st_size);
            
            // direct file transfer to socket
            sendfile(client_socket, file_descriptor, NULL, file_status.st_size);
            close(file_descriptor);
            printf("Served: %s (%ld bytes)\n", requested_file_path, file_status.st_size);
        } else {
            // Error handling
            printf("Error: 404 Not Found (%s)\n", requested_file_path);
            char *not_found_message = "<h1>404 Not Found</h1>";
            dprintf(client_socket, "HTTP/1.1 404 Not Found\r\nContent-Length: %ld\r\n\r\n%s", strlen(not_found_message), not_found_message);
        }
    }
    close(client_socket);
    return NULL;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    // hold the address information of the server
    struct sockaddr_in server_address = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = INADDR_ANY }; 
    
    // reusing port after restart
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    
    // binding the socket 
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) != 0 || listen(server_socket, 10) != 0) {
        perror("Server setup failed");
        return 1;
    }
    
    printf("Server running on port %d\n", PORT);
    
    // Connection handling
    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket >= 0) {
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, handle_client_request, (void*)(long)client_socket); // Create thread for new clients
            pthread_detach(client_thread);
        }
    }
    
    return 0; 
}



