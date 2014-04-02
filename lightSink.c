#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MAX_MESSLEN     102400

void usage() {
    printf("\n");
    printf("Usage: lightSink -h|? -u <user> -g <group> -s <server>\n");
    //    printf("Usage: sp_recv -h|? -u <user> -g <group> -s <server>\n");
    printf("\t-h|?\t\tHelp\n");
    printf("\t-l\t\tLoop.  Don't exit.\n");
    printf("\t-u <user>\tConnect to spread as user.\n");
    printf("\t-g <group>\tOn connect join group.\n");
    printf("\t-s <server>\tConnect to server, e.g 4803, 4803@host.\n");
    printf("\t-t n\tTime out after n seconds\n");
    printf("\t-p\t\tPoll\n");
    printf("\n");
}

void sig_handler(int signo) {
    if (signo == SIGALRM) {
        fprintf(stderr,"received SIGALRM\n");
    }
    exit(signo);
}

int main(int argc, char *argv[]) {
    int             ch;
    int             service_type = 0;
    int             ret;
    int             exitFlag = 0;
    int polling = 0;
    int verbose = 0;

    char            user[32];
    char            group[80];
    char            server[255];
    static char     message[MAX_MESSLEN];

    int             endian_mismatch;
    int16           mess_type;
    int timeout=0;
    int loop=0;

    int             num_groups;
    char            sender[MAX_GROUP_NAME];
    char            Private_group[MAX_GROUP_NAME];
    char            target_groups[100][MAX_GROUP_NAME];

    mailbox         Mbox;

    strcpy(user,"user");
    strcpy(server,"4803@localhost");
    group[0] = 0;

    while ((ch = getopt(argc, argv, "lh?u:g:s:t:pv")) != -1) {

        switch (ch) {
            case 'g':
                strcpy(group, optarg);
                break;
            case 'h':
            case '?':
                usage();
                exit(0);
                break;
            case 'l':
                loop=1;
                break;
            case 'p':
                polling = 1;
                break;
            case 's':
                strcpy(server, optarg);
                break;
            case 't':
                timeout=atoi(optarg);
                break;
            case 'u':
                strcpy(user, optarg);
                break;
            case 'v':
                verbose = 1;
                printf("Verbose set\n");
                break;
            default:
                usage();
                break;
        }
    }


    if(verbose) {
        printf("User  :%s\n",user);
        printf("Group :%s\n",group);
        printf("Server:%s\n",server);
    }
    ret = SP_connect(server, user, 0, 1, &Mbox, Private_group);

    if (ret < 0) {
        SP_error(ret);
        exit(1);
    }
    SP_join(Mbox, "global");

    if (strlen(group) > 0 ) {
        SP_join(Mbox, group);
    }

    if(polling) {
        ret = SP_poll( Mbox);
        if(ret <= 0)
            exit(0);
    }

    do {
        if(timeout > 0) {
            alarm(timeout);
        }
        signal(SIGALRM, sig_handler);

        ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(message), message);

        if (ret < 0) {
            SP_error(ret);
            exit(1);
        }

        if (Is_regular_mess(service_type)) {
            message[ret] = 0;
            printf("%s", message);

            if(loop ==0) {
                exitFlag = 1;
            } else {
                exitFlag=0;
            }
        }
    } while (!exitFlag);

    ret = SP_leave(Mbox, group);

    exit(0);

}
