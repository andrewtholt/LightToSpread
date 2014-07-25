#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage() {
    printf("\n");
    printf("Usage: lightSource -h|? -u <user> -g <group> -s <server> -m <message>\n");
    printf("\t-h|?\t\tHelp\n");
    printf("\t-u <user>\tConnect to spread as user.\n");
    printf("\t-g <group>\tOn connect join group.\n");
    printf("\t-s <server>\tConnect to server, e.g 4803, 4803@host.\n");
    printf("\t-m <message>\tSend the given message.  If not set read stdin.\n");
    printf("\t-l\t\tLeave the group prior to disconnect.\n");

    printf("\n");
    printf("The defaults are:\n");
    printf("\tlightSource -u user -g simple -s 4803\n");

    printf("\n");
}

int main( int argc, char *argv[] ) {
    int ch;
    int mflag = 0;
    int ret;
    char *tmp;
    int verbose = 0;
    int appendCr =0;
    int leave = 0;

    char user[32];
    char group[80];
    char server[255];
    char message[255];
    char    Private_group[MAX_GROUP_NAME];

    mailbox Mbox;

    strcpy(user,"user");
    strcpy(group,"simple");
    strcpy(server,"4803");

    if ( argc == 1) {
        usage();
        exit(0);
    }

    while((ch =getopt(argc,argv,"ch?u:g:s:lm:v")) != -1) {
        switch(ch) {
            case 'c':
                appendCr = 1;
                break;
            case 'h':
            case '?':
                usage();
                exit(0);
                break;
            case 'l':
                leave=1;
                break;
            case 'm':
                mflag=1;
                strcpy(message,optarg);
                break;
            case 's':
                strcpy(server,optarg);
                break;
            case 'u':
                strcpy(user,optarg);
                break;
            case 'g':
                strcpy(group,optarg);
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

    ret = SP_connect( server, user,0,1, &Mbox, Private_group );
    if( ret < 0) {
        SP_error ( ret );
        exit(1);
    }
    SP_join(Mbox, "global");

    SP_join(Mbox, group);

    while( mflag ==0) {
        if (!mflag) {
            tmp=fgets(message,255,stdin);
            if(tmp == (char *)NULL) {
                break;
            } else {
                ret = SP_multicast( Mbox, AGREED_MESS, group, 1, strlen(message), message );
            }
        }
    }
    if(mflag) {
        if(appendCr != 0) {
            strcat(message,"\n");
        }
        ret = SP_multicast( Mbox, AGREED_MESS, group, 1, strlen(message), message );
        
        if( ret < 0) {
            SP_error ( ret );
            exit(1);
        }
    }

    if (leave != 0) {
        ret = SP_leave( Mbox, group );
        ret = SP_leave( Mbox, "global" );
    }
    exit(0);
    
}
