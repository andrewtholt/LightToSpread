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
    printf("\t-c <filename>\tSet config file.\n");
    printf("\t-d\t\tEnable debugging messages.\n");
    printf("\t-h\t\tHelp\n");
    printf("\t-i\t\tInteractive mode.\n");
    printf("\t-g <name>\tSpread Group.\n");
    printf("\t-u <user>\tUser name.\n");
    printf("\t-s <server>\tSpread server.\n");

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

    global.connected=0;
    global.Group=(char *)NULL;
    global.configFileName=(char *)NULL;
    global.appDir=(char *)NULL;
    global.locked = 0;
    global.rawClient =1;

    global.debug = 0;
//    int runFlag=1;

    char *group=(char *)NULL;

#if FICL
    env=getenv("APPLIB");

    if((char *)NULL == env) {
        fprintf(stderr,"APPLIB not set, setting to default....\n");
        env = strsave("/usr/local/etc/lightToSpread/App");

        fprintf(stderr,"... %s\n",env);
//        exit(-1);
    }
    global.appDir = strsave(env);
    global.defaultGroup = strsave("global");

    global.system = ficlSystemCreate(NULL);
    ficlSystemCompileExtras(global.system);
    global.vm = ficlSystemCreateVm(global.system);
    ficlSystemCompileExtras(global.system);
    sprintf(cmdBuffer,"only forth also oop definitions seal\n");
    ficlVmEvaluate(global.vm, cmdBuffer);

#ifdef FICL
//    if( getFiclBoolean("TO_FORTH")) {
        if(loadFile("lib.fth") != 0) {
            fprintf(global.err,"lib.fth: Failed to load file.\n");
            exit(-3);
        }

        if(loadFile("classes.fth") != 0) {
            fprintf(global.err,"app_cfg.fth: Failed to load file.\n");
            exit(-3);
        }

        sprintf(cmdBuffer,"only forth also oop definitions seal\n");
        ficlVmEvaluate(global.vm, cmdBuffer);

//    }
#endif

    setFiclParam("TO_FORTH","false");
    setFiclParam("INTERACTIVE","false");
#else
    setFiclParam("INTERACTIVE","false");
    lockSymbol("INTERACTIVE");
#endif

    setFiclBoolean("ADD_SENDER",FALSE);
    setFiclParam("ADD_CR","true");
    setFiclParam("CMD_FORMAT","log $K $V");

    setFiclParam("AUTOCONNECT","false");
    setFiclParam("BUILD",__DATE__);
    mkLocal("BUILD");
    lockSymbol("BUILD");

    setFiclParam("CLIENT","raw");
    setFiclParam("DEBUG","false");
    setFiclParam("DEFAULT_GROUP","global");
    lockSymbol("DEFAULT_GROUP");
    
    setFiclParam("EXIT_ALLOWED","true");
    setFiclParam("GET_FORMAT","$K");
    setFiclParam("GROUP","");
    setFiclParam("MODE","local");
    setFiclParam("ON_CONNECT","");
    setFiclParam("SAVE_ALLOWED","true");
    setFiclParam("SET_FORMAT","$K $V");
    setFiclParam("SPREAD_SERVER","4803");

    setFiclParam("START_FILE","/dev/null");

    /*
    setFiclParam("STDERR","/dev/tty");
    setFiclParam("STDIN","/dev/tty");
    setFiclParam("STDOUT","/dev/tty");
    */

    setFiclParam("USER","source");

    while ((ch = getopt (argc, (char **) argv, "c:dhig:p:u:s:")) != -1) {
        switch(ch) {
            case 'c':
                setFiclParam("START_FILE",optarg);
                break;
            case 'd':
                setFiclParam("DEBUG","true");
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'i':
                setFiclParam("INTERACTIVE","true");
                break;
            case 'p':
                sprintf(buff,"%s.%%d", optarg);
                setFiclParam("PIPE_FMT",buff);
                mkLocal("PIPE_FMT");
                lockSymbol("PIPE_FMT");
                /*
                 * It makes no sense to go interactive when the i/o has
                 * been redirected, so prevent it.
                 */
                setFiclParam("INTERACTIVE","false");
                mkLocal("INTERACTIVE");
                lockSymbol("INTERACTIVE");
                break;
            case 'g':
                setFiclParam("GROUP",optarg);
                lockSymbol("GROUP");
                break;
            case 'u':
                setFiclParam("USER",optarg);
                lockSymbol("USER");
                break;
            case 's':
                setFiclParam("SPREAD_SERVER",optarg);
                lockSymbol("SPREAD_SERVER");
                break;
        }
    }
    /*
     * Read config file here.
     */
    loadSymbols();

    if( getFiclBoolean("DEBUG") != 0) {
        dumpSymbols();
    }

    ptr=getFiclParam("PIPE_FMT");
    if(ptr != (char *)NULL) {
        sprintf(buff,ptr,0);
        setFiclParam("STDIN",buff);
        mkfifo(buff,0600);

        sprintf(buff,ptr,1);
        setFiclParam("STDOUT",buff);
        mkfifo(buff,0600);
    }

    /*
#ifdef FICL
    if( getFiclBoolean("TO_FORTH")) {
        if(loadFile("lib.fth") != 0) {
            fprintf(global.err,"lib.fth: Failed to load file.\n");
            exit(-3);
        }

        if(loadFile("classes.fth") != 0) {
            fprintf(global.err,"app_cfg.fth: Failed to load file.\n");
            exit(-3);
        }

        sprintf(cmdBuffer,"only forth also oop definitions seal\n");
        ficlVmEvaluate(global.vm, cmdBuffer);

        if(loadFile("app_cfg.fth") != 0) {
            fprintf(global.err,"app_cfg.fth: Failed to load file.\n");
            exit(-3);
        }

        loadFile("local_cfgapp.fth");
    }
#endif   
*/
    loopFlag=getFiclBoolean("INTERACTIVE");

    printDebug("Opening STDIN ...");

    ptr = getFiclParam("STDIN");
    if(!ptr) {
        global.in=stdin;
    } else {
        global.in = fopen(getFiclParam("STDIN"),"r");
    }
    
    printDebug("Opening STDOUT ...");
    ptr = getFiclParam("STDOUT");
    if(!ptr) {
        global.out=stdout;
    } else {
        global.out = fopen(getFiclParam("STDOUT"),"w");
    }
    
    printDebug("Opening STDERR ...");
    ptr = getFiclParam("STDERR");
    if(!ptr) {
        global.err=stderr;
    } else {
        global.err = fopen(getFiclParam("STDERR"),"w");
    }
    printDebug("... done");

#ifdef FICL
    if(loadFile("app_cfg.fth") != 0) {
        fprintf(global.err,"app_cfg.fth: Failed to load file.\n");
        exit(-3);
    }

    loadFile("local_cfgapp.fth");
    while(loopFlag !=0) {
        printf("OK>");
        fflush(stdout);
        ptr=fgets(cmdBuffer,BUFFSIZE,stdin);
        ficlVmEvaluate(global.vm, cmdBuffer);

        tmp=getFiclParam("INTERACTIVE");

        if(!strcmp(tmp,"false")) {
            loopFlag=0;
        }

    }
#endif    

    if( getFiclBoolean("AUTOCONNECT") ) {
        connectToSpread();
    }

//    runFlag=1;
    group=getFiclParam("GROUP");
    
    if(strlen(group) == 0) {
//    if(!strcmp(group,"NOT-SET")) {
        group=getFiclParam("DEFAULT_GROUP");
    }

//    while(runFlag) {
    while(1) {
        if( global.connected ==0 ) {
            connectToSpread();
        }

//        while(runFlag) {
        while(global.connected != 0) {
            bzero(message,sizeof(message));
            bzero(buff,BUFFSIZE);

            fromIn(global.in,buff);
            if ( (cmdInterp(0, buff)) == 0) {
                if( global.connected != 0) {
                    if(strlen(buff) > 0) {
                        toSpread(group,buff);
                    } 
                }
            }
        }
        global.connected=0;
    }
    exit(0);
}
