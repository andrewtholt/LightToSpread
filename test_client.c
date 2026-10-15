#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SOCKET_PATH "/tmp/unixToSpread.sock"
#define BUFFSIZE 255

extern char *optarg;
extern int optind, opterr, optopt;

void usage(const char *progname) {
    printf("usage: %s [-h] [-s <socket_path>]\n\n", progname);
    printf("\t-h\t\tHelp.\n");
    printf("\t-s <socket>\tUnix domain socket path (default: %s).\n\n", SOCKET_PATH);
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_un server;
    char buffer[BUFFSIZE];
    fd_set read_fds;
    char *socket_path = SOCKET_PATH;
    int opt;

    while ((opt = getopt(argc, argv, "hs:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                exit(0);
            case 's':
                socket_path = optarg;
                break;
            default:
                usage(argv[0]);
                exit(1);
        }
    }

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        exit(1);
    }

    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, socket_path, sizeof(server.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_un)) < 0) {
        perror("connecting stream socket");
        close(sock);
        exit(1);
    }

    printf("Connected to unixToSpread server at %s\n", socket_path);
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
