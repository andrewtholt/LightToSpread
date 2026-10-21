/*
 * newSocketToSpread.c - TCP Network Socket Spread bridge using commonToSpread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "commonToSpread.h"

extern char *optarg;

void usage(void) {
    printf("usage: newSocketToSpread -h -c <cfg> -u <user> -d <database> -f -g <group> -l <logfile> -n <hostname> -p <port> -P\n\n");
    printf("\t-c <cfg>\tConfig file.\n");
    printf("\t-d <database>\tCache database.\n");
    printf("\t-f\t\tForce, remove default config.\n");
    printf("\t-g <group>\tSpread group.\n");
    printf("\t-h\t\tHelp.\n");
    printf("\t-l <logfile>\tLog file.\n");
    printf("\t-n <hostname>\tSet hostname.\n");
    printf("\t-p <port>\tTCP port to listen on (default: 9090).\n");
    printf("\t-u <user>\tSpread user.\n");
    printf("\t-P\t\tPrint config and exit.\n\n");
}

int main(int argc, const char *argv[]) {
    char buffer[BUFFSIZE];
    int ch;
    int dump_config = 0;
    int port = 9090;

    signal(SIGPIPE, SIG_IGN);
    initDefaultSymbols("newSocketToSpread");

    while ((ch = getopt(argc, (char **)argv, "hc:u:d:fg:l:n:p:P")) != -1) {
        if (handleCommonOption(ch, optarg, &dump_config)) {
            continue;
        }
        switch (ch) {
            case 'h':
                usage();
                exit(0);
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage();
                exit(1);
        }
    }

    finishSymbolInitialization();

    if (dump_config) {
        dumpSymbols();
        exit(0);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(1);
    }

    if (processConfigFile(getSymbol("START_FILE")) < 0) {
        fprintf(stderr, "A startup file ($HOME/.start.rc) MUST exist.\n");
        close(server_fd);
        exit(1);
    }

    if (debugOut) {
        fprintf(debugOut, "Listening on TCP port %d\n", port);
        fflush(debugOut);
    }

    int client_fd = -1;
    socklen_t addrlen = sizeof(address);

    while (runFlag) {
        if (client_fd < 0) {
            if (debugOut) {
                fprintf(debugOut, "Waiting for client connection on port %d...\n", port);
                fflush(debugOut);
            }
            client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (client_fd < 0) {
                if (runFlag) perror("accept failed");
                continue;
            }
            if (debugOut) {
                fprintf(debugOut, "Client connected.\n");
                fflush(debugOut);
            }

            int client_out_fd = dup(client_fd);
            myStdin = fdopen(client_fd, "r");
            myStdout = (client_out_fd >= 0) ? fdopen(client_out_fd, "w") : NULL;

            if (!myStdin || !myStdout) {
                perror("fdopen failed");
                if (myStdin) { fclose(myStdin); myStdin = NULL; }
                if (myStdout) { fclose(myStdout); myStdout = NULL; }
                if (client_out_fd >= 0) close(client_out_fd);
                close(client_fd);
                client_fd = -1;
                continue;
            }
        }

        if (fgets(buffer, BUFFSIZE, myStdin) == NULL) {
            if (myStdout) {
                fflush(myStdout);
                fclose(myStdout);
                myStdout = NULL;
            }
            if (myStdin) {
                fclose(myStdin);
                myStdin = NULL;
            }
            client_fd = -1;
            if (debugOut) {
                fprintf(debugOut, "Client disconnected.\n");
                fflush(debugOut);
            }
            continue;
        }

        processSpreadCommand(buffer, &runFlag);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
