#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SOCKET_PATH "/tmp/toSpread.sock"
#define BUFFSIZE 255

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_un server;
    char buffer[BUFFSIZE];
    fd_set read_fds;
    char *socket_path = SOCKET_PATH;

    if (argc > 1) {
        socket_path = argv[1];
    }

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        exit(1);
    }

    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, socket_path);

    if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_un)) < 0) {
        perror("connecting stream socket");
        close(sock);
        exit(1);
    }

    printf("Connected to toSpread server at %s\n", socket_path);
    printf("Type commands (e.g., ^help, ^start, ^dump, ^exit):\n> ");
    fflush(stdout);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sock, &read_fds);

        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }
            if (write(sock, buffer, strlen(buffer)) < 0) {
                perror("write");
                break;
            }
            printf("> ");
            fflush(stdout);
        }

        if (FD_ISSET(sock, &read_fds)) {
            int n = read(sock, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                printf("\nServer disconnected.\n");
                break;
            }
            buffer[n] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }
    }

    close(sock);
    return 0;
}
