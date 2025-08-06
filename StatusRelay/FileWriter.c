#include <stdio.h>        // For printf, perror
#include <stdlib.h>       // For exit()
#include <string.h>       // For strlen()
#include <unistd.h>       // For close()
#include <sys/socket.h>   // For socket functions
#include <netinet/in.h>   // For sockaddr_in structure
#include <arpa/inet.h>    // For htons(), inet_addr()
#include <unistd.h>       // For sleep()
#include <poll.h>         // For poll()
#include <fcntl.h>        // For file locking
#include <sys/file.h>     // For file locking


#define DEBUG 0
#define NUM_GPUS 4
#define NUM_CPUS 2

typedef struct {
    int temp;
    int vram;
} gpu_state;

typedef struct {
    int temp;
} cpu_state;

typedef struct {
    cpu_state cpu[NUM_CPUS];
    gpu_state gpu[NUM_GPUS];
} system_state;

void initSystem(system_state* target) {
    for (int i = 0; i < NUM_CPUS; i++) {
        (target->cpu+i)->temp = 0;
    }
    for (int i = 0; i < NUM_GPUS; i++) {
        (target->gpu+i)->temp = 0;
        (target->gpu+i)->vram = 0;
    }
}

void updateFile(system_state* target) {
    int file = open("systemState.txt", O_WRONLY | O_CREAT, 0644);
    ftruncate(file, 0);
    if (flock(file, LOCK_EX | LOCK_NB) == 0) {
        if (DEBUG) {printf("successfully locked systemState.txt!\n");}
    } else {
        printf("could not obtain lock on fd: %d\n");
        return;
    }   

    char buffer[100];
    for (int i = 0; i < NUM_CPUS; i++) {
        sprintf(buffer, "cpu %d temp: %d\n", i, (target->cpu+i)->temp);
        write(file, buffer, strlen(buffer));
    }
    for (int i = 0; i < NUM_GPUS; i++) {
        sprintf(buffer, "gpu %d temp: %d\n", i, (target->gpu+i)->temp);
        write(file, buffer, strlen(buffer));
        sprintf(buffer, "gpu %d vram: %d\n", i, (target->gpu+i)->vram);
        write(file, buffer, strlen(buffer));
    }    

    flock(file, LOCK_UN);
    if (DEBUG) {printf("unlocked systemState.txt!\n");}

    close(file);

}

int main() {

    int server_running = 1;

    int server_fd, client_fd;           // File descriptors for socket
    struct sockaddr_in server_addr, client_addr;  // Address structures
    socklen_t client_len = sizeof(client_addr);  // Size of address
    char buffer[1024];                  // Buffer to hold received data
    
    // Step 1: Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    // socket() creates a new socket
    // AF_INET = IPv4 protocol
    // SOCK_STREAM = TCP (reliable, connection-based)
    // 0 = default protocol (automatically set to TCP)
    
    // Check if socket creation was successful
    if (server_fd < 0) {
        perror("socket creation failed");  // Print error message
        exit(EXIT_FAILURE);             // Exit program
    }
    
    // Step 2: Configure server address
    server_addr.sin_family = AF_INET;           // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Listen on all interfaces
    server_addr.sin_port = htons(8080);       // Port 8080 (converted to network byte order)
    
    // Step 3: Bind socket to address
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Step 4: Listen for connections
    if (listen(server_fd, 3) < 0) {  // 3 = max queue of pending connections
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port 8080\n");

    //settup for system state struct
    char part[20];
    char type[20];
    int index = 0;
    int data = 0;
    system_state currentState;
    initSystem(&currentState);

    //settup for polling stdin
    struct pollfd key_in;
    key_in.fd = STDIN_FILENO;    // Check keyboard input
    key_in.events = POLLIN;       // We want to know if data comes in

    //settup for polling client_fd
    struct pollfd client_in;
    //we don't assign .fd as we haven't gotten the connection yet
    client_in.events = POLLIN;

    //settup for polling server_fd
    struct pollfd server_in;
    server_in.fd = server_fd;
    server_in.events = POLLIN;

    int active_connection = 0;
    // Main server loop with signal handling
    while (server_running) {
        // Step 5: Accept client connection
        if (DEBUG) {printf("checking server_in...\n");}
        if ((poll(&server_in, 1, 0) > 0) && (active_connection == 0)) { //if poll returns greater than 0 than it is ready for reading, otherwise we skip getting new connection to prevent blocking
            client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }
            client_in.fd = client_fd; //complete settup for polling
            active_connection = 1;
            // accept() waits for and accepts a connection
            // Returns a new socket for the client connection
        }        

        int bytes_received;

        if (active_connection == 1) {
            if (DEBUG) {printf("checking client_in...\n");}
            if (poll(&client_in, 1, 0) > 0) { //if poll returns greater than 0 than it is ready for reading, otherwise we skip reading to prevent blocking
                bytes_received = recv(client_fd, buffer, sizeof(buffer), 0); //do the actual reading
                if (bytes_received==0) {
                    close(client_fd);   // Close client socket
                    active_connection = 0;
                }
                for (int i = 0; i < bytes_received; i++) {
                    printf("%c", *(buffer+i));
                }
                sscanf(buffer, "%s %s %d %d", &part, &type, &index, &data);

                if (strcmp(part,"gpu") == 0) {
                    if (strcmp(type,"temp") == 0) {
                        (currentState.gpu+index)->temp = data;
                    } else if (strcmp(type,"vram") == 0) {
                        (currentState.gpu+index)->vram = data;
                    }
                } else if (strcmp(part,"cpu") == 0) {
                    if (strcmp(type,"temp") == 0) {
                        (currentState.cpu+index)->temp = data;
                    } 
                }

                send(client_fd, buffer, bytes_received, 0);  // Send it back
            }
        }

        if (DEBUG == 1) {
            for (int i = 0; i < NUM_CPUS; i++) {
                printf("cpu %d at temp: %d\n", i, (currentState.cpu+i)->temp);
            }


            for (int i = 0; i < NUM_GPUS; i++) {
                printf("gpu %d at temp: %d\n", i, (currentState.gpu+i)->temp);
                printf("gpu %d at vram: %d\n", i, (currentState.gpu+i)->vram);
            }
        }

        updateFile(&currentState);

        int result = poll(&key_in, 1, 0);  // check if keyboard has stuff to read
        if (result > 0) { //if it does we shmoove
            if (active_connection == 1) {
                close(client_fd);
            }
            while ( getchar() != '\n' ) {;} //clear stdin
            break;
        }



        sleep(1); //set a speed limit for the loop so we don't flood the terminal with printf

    }

    // Clean shutdown
    close(server_fd);  // Close server socket
    printf("Server shut down cleanly\n");

    return 0;
}
