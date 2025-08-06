#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[50];
    char *message = "Hello, Server!";
    
    // Create socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    
    // Send message

    int fd = open("/sys/class/thermal/thermal_zone0/temp", O_RDONLY);
    if (fd==-1) {
        printf("failed to open file!\n");
        return 1;
    }
    char formattedBuffer[50];
    read(fd, buffer, 49); //same deal for only reading 49 a \n is appended at the end (also why we don't put a \n in the sprintf below!)
    sprintf(formattedBuffer, "cpu temp 0 %s", buffer);
    formattedBuffer[strlen(formattedBuffer)-4] = '\n'; //chop of last 3 digits of the string (since the value is recorded in millicelcius this is like dividing by 1000 and then flooring)
    formattedBuffer[strlen(formattedBuffer)-3] = '\0'; //jank? yes! but working? also yes!
    printf("Sending: %s", formattedBuffer);

    send(client_fd, formattedBuffer, strlen(formattedBuffer), 0);
    
    // Receive echo
    memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer) - 1, 0); //the -1 is because a \n is automatically appended to the message.
    
    printf("Server echoed: %s", buffer);
    
    close(client_fd);
    return 0;
}