#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <semaphore.h>

#include <termios.h>
struct termios orig_termios; 
char prompt[32];
char *loadPath;

#ifdef MAC
#include <sys/sysctl.h>
#endif

#include "sp.h"

#define MAX_MESSLEN     102400
#define BUFFSIZE 255

typedef struct {
    int delay;
} param;

#include "hash.h"
#include "mine.h"
#include "connectToSpread.h"

struct globalDefinitions global;

void usage(void) {
    printf("Usage:\n\n");
    printf("\t-d\t\tEnable debugging messages.\n");
    printf("\t-h\t\tHelp\n");
    printf("\t-g <name>\tSpread Group.\n");
    printf("\t-r <server>\tRedis server, format <name:socket>.\n");
    printf("\t-s <server>\tSpread server, format <socket@hostname>.\n");
    printf("\t-u <user>\tUser name.\n");
    printf("\t-v\t\tVerbose.\n");

    printf("\n\nDefault usage is equivalent to:\n");
    printf("\tredisListener -u redis -s 4803@localhost -r localhost:6379\n\n");

}

int main(int argc, char* argv[]) {

    extern char * optarg;

    int ch;
    char *env=(char *)NULL;
    char *ptr;

    int loopFlag=1;

    char cmdBuffer[BUFFSIZE];
    char buff[BUFFSIZE];
    char ficlResult[BUFFSIZE];

    struct cString *tmp;

    int runFlag=1;

    int service_type=0;
    int num_groups;
    char sender[BUFFSIZE];
    char target_groups[100][MAX_GROUP_NAME];
    int16 mess_type;
    static char message[MAX_MESSLEN];
    int endian_mismatch;
    int ret;
    char *tok;

    global.verbose=0;
    global.debug=0;

    defaultSymbols();

    setSymbolValue("DEFAULT_GROUP", "global");
    setSymbolValue("USER", "redis");
    setSymbolValue("REDIS_HOSTNAME","localhost");
    setSymbolValue("REDIS_SOCKET","6379");

    while ((ch = getopt (argc, (char **) argv, "dg:hu:r:s:vV")) != -1) {
        switch(ch) {
            case 'd':
                global.debug=1;
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'g':
                setSymbolValue("GROUP", optarg);
                break;
            case 'r':
                tok = strtok(optarg,":");
                if(!tok) {
                    setSymbolValue("REDIS_HOSTNAME","localhost");
                } else {
                    setSymbolValue("REDIS_HOSTNAME",tok);
                }
                tok = strtok(NULL," \n:" );
                if(!tok) {
                    setSymbolValue("REDIS_SOCKET","6379");
                } else {
                    setSymbolValue("REDIS_SOCKET",tok);
                }

                break;
            case 's':
                setSymbolValue("SPREAD_SERVER",optarg);
                break;
            case 'u':
                setSymbolValue("USER", optarg);
                break;
            case 'V':
            case 'v':
                global.verbose=1;
                fprintf(stderr,"Verbose Mode\n");
                break;
        }
    }

    if( global.verbose ) {
        dumpInteractive();
    }


    connectToSpread();
    while(1) {
        runFlag=1;

        while(runFlag) {
            bzero(message,sizeof(message));
            bzero(buff,BUFFSIZE);
            /*
            ret = SP_receive (global.Mbox, &service_type, sender, 100,
                    &num_groups, target_groups,
                    &mess_type, &endian_mismatch, sizeof (message),
                    message);
                    */

            service_type=fromSpread(sender,message);

            if (ret >= 0) {
                if (Is_regular_mess (service_type)) {
//                    fromSpread(sender,message);
                    printf("Sender:%s\tMessage:%s\n",sender,message);
                } else if (Is_membership_mess (service_type)) {
                } else if (Is_reg_memb_mess (service_type)) {
                } else if (Is_caused_join_mess (service_type)) {
                } else if (Is_caused_leave_mess (service_type)) {
                } else if (Is_caused_disconnect_mess (service_type)) {
                } else if (Is_caused_network_mess (service_type)) {
                }


                /*
                if(strlen(buff) > 0) {
                    toOut(buff);
                }
                */
            } else {
                SP_error(ret);
                runFlag=0;
            }
        }
    }
    exit(0);
}
