#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define DEFAULT_PORT 9090
#define BUFFSIZE 255

extern char *optarg;

void usage(const char *progname) {
    printf("usage: %s [-h] [-p <port>] [-H <host>]\n\n", progname);
    printf("\t-h\t\tHelp.\n");
    printf("\t-p <port>\tServer port (default: %d).\n", DEFAULT_PORT);
    printf("\t-H <host>\tServer host (default: 127.0.0.1).\n\n");
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server;
    char buffer[BUFFSIZE];
    fd_set read_fds;
    int port = DEFAULT_PORT;
    char *host = "127.0.0.1";
    int opt;

    while ((opt = getopt(argc, argv, "hp:H:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                exit(0);
            case 'p':
                port = atoi(optarg);
                break;
            case 'H':
                host = optarg;
                break;
            default:
                usage(argv[0]);
                exit(1);
        }
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening socket");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server.sin_addr) <= 0) {
        perror("invalid address/ address not supported");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connecting to server");
        close(sock);
        exit(1);
    }

    printf("Connected to socketToSpread server at %s:%d\n", host, port);
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
