#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>

#if FICL
#include <ficl.h>
#endif

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
    printf("\t-u <user>\tUser name.\n");
    printf("\t-s <server>\tSpread server.\n");
    printf("\t-v\t\tVerbose.\n\n");
    printf("\t-C <cmd>\tRedis Cmd\n");
    printf("\t-K <key>\tRedis Key\n");
    printf("\t-V <value>\tRedis Value\n");
    printf("\t-H\t\tUse the redis HSET command, not SET.  Like sending\n");
    printf("\t\t\tHSET <key> VALUE <value>\n\n");

    printf("\nDefault usage is equivalent to:\n");
    printf("\tsource -u source -s 4803 -c /dev/null\n\n");

}

int main(int argc, char* argv[]) {
    // int rc=0;

    static char message[MAX_MESSLEN];
    int ch;
    char *env=(char *)NULL;
    char *ptr;

    int loopFlag=1;

    char cmdBuffer[BUFFSIZE];
    char buff[BUFFSIZE];

    char *tmp=(char *)NULL;

    int hset=0;

    global.connected=0;
    //    global.locked = 0;
    global.debug = 0;
    global.verbose = 0;

    char *group = (char *)NULL;
    char *cmd   = (char *)NULL;
    char *key   = (char *)NULL;
    char *value = (char *)NULL;

    setSymbolValue("USER","source");

    while ((ch = getopt (argc, (char **) argv, "C:dhg:u:vs:HK:V:")) != -1) {
        switch(ch) {
            case 'C':
                cmd=strsave( optarg );
                break;
            case 'd':
                global.debug=1;
                break;
            case 'g':
                setSymbolValue("GROUP",optarg);
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'u':
                setSymbolValue("USER",optarg);
                lockSymbol("USER");
                break;
            case 's':
                setSymbolValue("SPREAD_SERVER",optarg);
                lockSymbol("SPREAD_SERVER");
                break;
            case 'v':
                global.verbose=1;
                break;
            case 'K':
                key=strsave( optarg );
                break;
            case 'V':
                value=strsave( optarg );
                break;
            case 'H':
                hset=1;
                break;
        }
    }

    /*
    if( (key == (char *)NULL) || (value == (char *)NULL) ) {
        usage();
        exit(1);
    }
    */

    if( global.debug != 0) {
        dumpSymbols();
    }


    group=getSymbol("GROUP");

    while(global.connected == 0) {
        connectToSpread();
    }

    bzero(message,sizeof(message));
    bzero(buff,BUFFSIZE);

    if( (cmd == NULL) || (!strcmp( cmd,"SET" ))) {
        if (hset == 0) {
            redisSet(key,value,message);
        } else {
            redisHset(key,value,message);
        }
    } else {
        if(!strcmp("GET", cmd)) {
            redisGet(key,message);
        }
        else if(!strcmp("SUB", cmd)) {
            redisSub(key,message);
        }
    }

    //    fromIn(global.in,buff);
    //    if ( (cmdInterp(0, buff)) == 0) {
    if( global.connected != 0) {
        if(strlen(message) > 0) {
            toSpread(group,message);
        } 
    }
    //    }
    exit(0);
}
