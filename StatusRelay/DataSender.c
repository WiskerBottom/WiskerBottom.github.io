#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>


#define SERVER_IP "192.168.86.110"
#define SERVER_PORT 9478

void sendData(int index, char* type, int target_fd) {
    char buffer[50];
    char formattedBuffer[50];
    if (strcmp(type, "cpu") == 0) {
        sprintf(formattedBuffer, "/sys/class/thermal/thermal_zone%d/temp", index);
        int fd = open(formattedBuffer, O_RDONLY);
        if (fd==-1) {
            printf("failed to open file!\n");
            return;
        }
        memset(buffer, 0, sizeof(buffer)); //if we don't zero out the buffers before my manual string editing later doesn't work ;(
        memset(formattedBuffer, 0, sizeof(buffer));
        read(fd, buffer, 49); //same deal for only reading 49 a \n is appended at the end (also why we don't put a \n in the sprintf below!)
        sprintf(formattedBuffer, "cpu temp %d %s", index, buffer);
        formattedBuffer[strlen(formattedBuffer)-4] = '\n'; //chop of last 3 digits of the string (since the value is recorded in millicelcius this is like dividing by 1000 and then flooring)
        formattedBuffer[strlen(formattedBuffer)-3] = '\0'; //jank? yes! but working? also yes!
        close(fd);
    } else if (strcmp(type, "gpu") == 0) {
        system("rocm-smi --showmemuse --csv > rocm_output.txt");
        int fd = open("rocm_output.txt", O_RDONLY);
        if (fd==-1) {
            printf("failed to open file!\n");
            return;
        }
        buffer[0] == 'c'; //to prevent the first chacter in the buffer randomly being ','
        for (int i = 0; i < (3 + 2 * index); i++) { //loop 3 times to get to the after the 3rd comma
            while (buffer[0] != ',') {
                read(fd, buffer, 1);
                //printf("current char being analysed: %c\n", buffer[0]);
            }
            buffer[0] = 'c'; //as we just found a comma we need to change it to something else so when we come back round next time we don't immediately see it again.
        }
        read(fd, buffer, 3); //percentage goes from 0 - 100 so we read the next 3 bytes

        //chop off anything after the next comma (comma also gets chopped)
        int i = 0;
        while (buffer[i] != ',') {
            //printf("current char being analysed (2): %c\n", buffer[i]);
            i++;
        }
        buffer[i] = '\0';

        sprintf(formattedBuffer, "gpu temp %d %s\n", index, buffer); //use sprtinf to remove potential trailing junk
        close(fd);
    } else {
        return;
    }

    printf("Sending: %s", formattedBuffer);
    send(target_fd, formattedBuffer, strlen(formattedBuffer), 0);
    
    // Receive echo
    memset(buffer, 0, sizeof(buffer));
    recv(target_fd, buffer, sizeof(buffer) - 1, 0); //the -1 is because a \n is automatically appended to the message.
    
    printf("Server echoed: %s", buffer);
}

int main() {
    int client_fd;
    struct sockaddr_in server_addr;

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
    sendData(1, "gpu", client_fd);
    sendData(3, "gpu", client_fd);
    sendData(0, "cpu", client_fd);
    
    close(client_fd);
    return 0;
}