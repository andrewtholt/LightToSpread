/*
 * newUnixToSpread.c - Unix Domain Socket Spread bridge using commonToSpread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>

#include "commonToSpread.h"

extern char *optarg;

enum {
    OPT_FOREGROUND = 1000
};

static struct option long_options[] = {
    {"foreground", no_argument, NULL, OPT_FOREGROUND},
    {"help",       no_argument, NULL, 'h'},
    {NULL,         0,           NULL, 0}
};

void usage(void) {
    printf("usage: newUnixToSpread -h -c <cfg> -u <user> -d <database> -f -g <group> -l <logfile> -n <hostname> -s <socket> -P -r --foreground\n\n");
    printf("\t-c <cfg>\tConfig file.\n");
    printf("\t-d <database>\tCache database.\n");
    printf("\t-f\t\tForce, remove default config.\n");
    printf("\t-g <group>\tSpread group.\n");
    printf("\t-h\t\tHelp.\n");
    printf("\t-l <logfile>\tLog file (default: /tmp/newUnixToSpread.log).\n");
    printf("\t-n <hostname>\tSet hostname.\n");
    printf("\t-s <socket>\tUnix domain socket path (default: /tmp/unixToSpread.sock).\n");
    printf("\t-u <user>\tSpread user.\n");
    printf("\t-P\t\tPrint config and exit.\n");
    printf("\t-r\t\tSet user to a random (pid) name, in the form r<pid>.\n");
    printf("\t--foreground\tRun in foreground and output to console.\n\n");
}

int main(int argc, const char *argv[]) {
    char buffer[BUFFSIZE];
    int ch;
    int dump_config = 0;
    char *socketPath = NULL;
    int foreground = 0;

    signal(SIGPIPE, SIG_IGN);
    initDefaultSymbols("newUnixToSpread");

    while ((ch = getopt_long(argc, (char **)argv, "hc:u:d:fg:l:n:s:Pr", long_options, NULL)) != -1) {
        if (handleCommonOption(ch, optarg, &dump_config)) {
            continue;
        }
        switch (ch) {
            case 'h':
                usage();
                exit(0);
            case 's':
                socketPath = strsave(optarg);
                break;
            case OPT_FOREGROUND:
                foreground = 1;
                break;
            default:
                usage();
                exit(1);
        }
    }

    if (!socketPath) {
        socketPath = strsave("/tmp/unixToSpread.sock");
    }

    if (!foreground && !getSymbol("LOG_FILE")) {
        setSymbol("LOG_FILE", "/tmp/newUnixToSpread.log", UNLOCK, LOCAL);
    }

    finishSymbolInitialization();

    if (dump_config) {
        dumpSymbols();
        exit(0);
    }

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, socketPath, sizeof(address.sun_path) - 1);

    unlink(socketPath);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        unlink(socketPath);
        exit(1);
    }

    char *startFile = getSymbol("START_FILE");
    if (!startFile || access(startFile, R_OK) != 0) {
        fprintf(stderr, "A startup file ($HOME/.start.rc) MUST exist.\n");
        close(server_fd);
        unlink(socketPath);
        exit(1);
    }

    if (!foreground) {
        const char *logfile = getSymbol("LOG_FILE");
        int log_fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd < 0) {
            fprintf(stderr, "Failed to open log file %s: %s\n", logfile, strerror(errno));
            close(server_fd);
            unlink(socketPath);
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(log_fd);
            close(server_fd);
            unlink(socketPath);
            exit(1);
        }

        if (pid > 0) {
            /* Parent process exits cleanly */
            close(server_fd);
            close(log_fd);
            exit(0);
        }

        /* Child process runs in background */
        if (setsid() < 0) {
            perror("setsid failed");
            close(log_fd);
            close(server_fd);
            unlink(socketPath);
            exit(1);
        }

        /* Redirect stdin to /dev/null */
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (devnull > STDERR_FILENO) {
                close(devnull);
            }
        }

        /* Redirect stdout and stderr to the logfile */
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        if (log_fd > STDERR_FILENO) {
            close(log_fd);
        }

        if (!debugOut || debugOut == stderr) {
            debugOut = stderr;
        }

        setvbuf(stdout, NULL, _IOLBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    if (processConfigFile(getSymbol("START_FILE")) < 0) {
        fprintf(stderr, "A startup file ($HOME/.start.rc) MUST exist.\n");
        close(server_fd);
        unlink(socketPath);
        exit(1);
    }

    if (debugOut) {
        fprintf(debugOut, "Listening on Unix domain socket: %s\n", socketPath);
        fflush(debugOut);
    }

    int client_fd = -1;
    socklen_t addrlen = sizeof(address);

    while (runFlag) {
        if (client_fd < 0) {
            if (debugOut) {
                fprintf(debugOut, "Waiting for client connection on %s...\n", socketPath);
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

        if (processSpreadCommand(buffer, &runFlag) == CMD_DISCONNECT) {
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
    }

    close(server_fd);
    unlink(socketPath);
    return EXIT_SUCCESS;
}
