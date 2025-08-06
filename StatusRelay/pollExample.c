#include <poll.h>
#include <unistd.h>
#include <stdio.h>

int main() {

    // You want to check if there's input on the keyboard (stdin)
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;    // Check keyboard input
    pfd.events = POLLIN;       // We want to know if data comes in

    // Check NOW - but don't wait

    while (1) {
        sleep(1);
        printf("haiiii\n");
        int result = poll(&pfd, 1, 0);  // Check once, don't wait
        if (result > 0) {
            printf("we leave\n");
            break;
        }
    }

}