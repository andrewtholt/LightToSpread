/*
 * newToSpread.c - stdin/stdout and FIFO pipe Spread bridge using commonToSpread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "commonToSpread.h"

extern char *optarg;

void usage(void) {
    printf("usage: newToSpread -h -c <cfg> -u <user> -d <database> -f -g <group> -l <logfile> -n <hostname> -i <stdinFifo> -o <stdoutFifo> -P\n\n");
    printf("\t-c <cfg>\tConfig file.\n");
    printf("\t-d <database>\tCache database.\n");
    printf("\t-f\t\tForce, remove default config.\n");
    printf("\t-g <group>\tSpread group.\n");
    printf("\t-h\t\tHelp.\n");
    printf("\t-i <stdinFifo>\tListen to this fifo.\n");
    printf("\t-l <logfile>\tLog file.\n");
    printf("\t-n <hostname>\tSet hostname.\n");
    printf("\t-o <stdoutFifo>\tWrite to this fifo.\n");
    printf("\t-u <user>\tSpread user.\n");
    printf("\t-P\t\tPrint config and exit.\n\n");
}

int main(int argc, const char *argv[]) {
    char buffer[BUFFSIZE];
    char fifoScratch[255];
    char *stdinFifo = NULL;
    char *stdoutFifo = NULL;
    int ch;
    int dump_config = 0;
    FILE *fp;

    fifoScratch[0] = '\0';
    initDefaultSymbols("newToSpread");

    while ((ch = getopt(argc, (char **)argv, "hc:u:d:fg:l:n:p:i:o:P")) != -1) {
        if (handleCommonOption(ch, optarg, &dump_config)) {
            continue;
        }
        switch (ch) {
            case 'h':
                usage();
                exit(0);
            case 'p':
                snprintf(fifoScratch, sizeof(fifoScratch), "%s.%%d", optarg);
                break;
            case 'i':
                stdinFifo = strsave(optarg);
                break;
            case 'o':
                stdoutFifo = strsave(optarg);
                break;
            default:
                usage();
                exit(1);
        }
    }

    if (stdinFifo) {
        if (access(stdinFifo, F_OK) == -1) {
            if (mkfifo(stdinFifo, 0666) == -1) {
                perror("mkfifo for stdin failed");
                exit(1);
            }
        }
    }
    if (stdoutFifo) {
        if (access(stdoutFifo, F_OK) == -1) {
            if (mkfifo(stdoutFifo, 0666) == -1) {
                perror("mkfifo for stdout failed");
                exit(1);
            }
        }
    }

    if (stdoutFifo) {
        myStdout = fopen(stdoutFifo, "w");
        if (!myStdout) {
            perror("fopen for stdout fifo failed");
            exit(1);
        }
    }
    if (stdinFifo) {
        myStdin = fopen(stdinFifo, "r");
        if (!myStdin) {
            perror("fopen for stdin fifo failed");
            exit(1);
        }
    }

    finishSymbolInitialization();

    if (dump_config) {
        dumpSymbols();
        exit(0);
    }

    if (processConfigFile(getSymbol("START_FILE")) < 0) {
        fprintf(stderr, "A startup file MUST exist, even if it is empty\n");
        fprintf(stderr, "The default name is $HOME/.start.rc\n");
        exit(1);
    }

    fp = myStdin;
    while (runFlag) {
        char *prompt = getSymbol("PROMPT");
        if (prompt && strcmp(prompt, "NONE") != 0 && stdinFifo == NULL) {
            printf("%s", prompt);
            fflush(stdout);
        }

        if (fgets(buffer, BUFFSIZE, fp) == NULL) {
            if (feof(fp)) {
                if (fp == myStdin && stdinFifo != NULL) {
                    fclose(fp);
                    if (debugOut) {
                        fprintf(debugOut, "Client disconnected. Waiting for new connection on %s\n", stdinFifo);
                        fflush(debugOut);
                    }
                    fp = fopen(stdinFifo, "r");
                    if (!fp) {
                        perror("Failed to reopen stdin FIFO, exiting.");
                        runFlag = 0;
                    } else {
                        myStdin = fp;
                        if (debugOut) {
                            fprintf(debugOut, "New client connected.\n");
                            fflush(debugOut);
                        }
                    }
                    continue;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        processSpreadCommand(buffer, &runFlag);
    }

    return EXIT_SUCCESS;
}
