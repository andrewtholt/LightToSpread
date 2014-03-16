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

#ifdef FICL
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
    printf("\tsink -u sink -s 4803 -c /dev/null\n\n");

}

int main(int argc, char* argv[]) {
    int ch;
    char *env=(char *)NULL;
    char *ptr;

    int loopFlag=1;

    char cmdBuffer[BUFFSIZE];
    char buff[BUFFSIZE];
    char ficlResult[BUFFSIZE];

    struct cString *tmp;

    global.connected=0;
    global.Group=(char *)NULL;
    global.configFileName=(char *)NULL;
    global.appDir=(char *)NULL;
    global.locked = 0;

    global.debug = 0;
    global.rawClient = 0;
    global.redisClient = 0;
    global.formatClient = 0;

    int runFlag=1;

    int service_type=0;
    int num_groups;
    char sender[BUFFSIZE];
    char target_groups[100][MAX_GROUP_NAME];
    int16 mess_type;
    static char message[MAX_MESSLEN];
    int endian_mismatch;
    int ret;

#ifdef FICL
    bzero(ficlResult,BUFFSIZE);
    global.system = ficlSystemCreate(NULL);
    ficlSystemCompileExtras(global.system);
    global.vm = ficlSystemCreateVm(global.system);
    ficlSystemCompileExtras(global.system);

    sprintf(cmdBuffer,"only forth also oop definitions seal\n");
    ficlVmEvaluate(global.vm, cmdBuffer);

    sprintf(cmdBuffer,"%d constant result-buffer\n",(int)ficlResult);
    ficlVmEvaluate(global.vm, cmdBuffer);

    if(loadFile("lib.fth") != 0) {
        fprintf(stderr,"lib.fth: Failed to load file.\n");
        exit(-3);
    }

    if(loadFile("classes.fth") != 0) {
        fprintf(stderr,"app_cfg.fth: Failed to load file.\n");
        exit(-3);
    }

    setFiclParam("INTERACTIVE","false");
    setFiclParam("TO_FORTH","false");
#else
    setFiclParam("INTERACTIVE","false");
    lockSymbol("INTERACTIVE");
#endif

    setFiclParam("ADD_CR","true");
//    setFiclParam("APP_DIR","");
//    mkLocal("APP_DIR");
    setFiclParam("AUTOCONNECT","false");
    setFiclParam("BUILD",__DATE__);
    mkLocal("BUILD");
    lockSymbol("BUILD");

    setFiclParam("CLIENT","raw");
    setFiclParam("SET_FORMAT","$K $V");

    setFiclParam("CMD_FORMAT","log $K $V");

    setFiclBoolean("DEBUG",FALSE);
    setFiclBoolean("ADD_SENDER",FALSE);

    setFiclParam("DEFAULT_GROUP","global");
    lockSymbol("DEFAULT_GROUP");

    setFiclParam("EXIT_ALLOWED","true");
    setFiclParam("GET_FORMAT","$K");
    setFiclParam("GROUP","");
    setFiclParam("MODE","local");
    setFiclParam("ON_CONNECT","");

    setFiclParam("SAVE_ALLOWED","true");
    setFiclParam("SPREAD_SERVER","4803");
    setFiclParam("START_FILE","/dev/null");
    setFiclParam("USER","sink");

    while ((ch = getopt (argc, (char **) argv, "c:dhig:p:u:s:")) != -1) {
        switch(ch) {
            case 'c':
                setFiclParam("START_FILE",optarg);
                lockSymbol("START_FILE");
                break;
            case 'd':
                setFiclParam("DEBUG","true");
                lockSymbol("DEBUG");
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'i':
                setFiclParam("INTERACTIVE","true");
                lockSymbol("INTERACTIVE");
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
                // 
                // Also set autoconnect ?
                //
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


    if(getFiclBoolean("TO_FORTH")) {
        env=getenv("APPLIB");
        if((char *)NULL == env) {
            fprintf(stderr,"Environment variable APPLIB not set, goodbye.\n");
            env = strsave("/usr/local/etc/lightToSpread/App");
        } 
        setFiclParam("APP_DIR", env);
    }


    if( getFiclBoolean("DEBUG") != 0) {
        dumpSymbols();
        dumpGlobals();
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


#ifdef FICL
    if(loadFile("app_cfg.fth") != 0) {
        fprintf(stderr,"app_cfg.fth: Failed to load file.\n");
        exit(-3);
    }

    loadFile("local_cfgapp.fth");
#endif
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
    while(loopFlag !=0) {
        printf("OK>");
        fflush(stdout);
        ptr=fgets(cmdBuffer,BUFFSIZE,stdin);
        ficlVmEvaluate(global.vm, cmdBuffer);

        tmp=getSymbol("INTERACTIVE");

        if(!strcmp(tmp->text,"false")) {
            loopFlag=0;
        }

    }
#endif

    if( getFiclBoolean("AUTOCONNECT") ) {
        connectToSpread();
    } else {
        while(global.connected == 0) {
            fromIn(global.in,buff);
            cmdInterp(0, buff);
        }

    }

    while(global.connected == 0) {
        fromIn(global.in,buff);
        cmdInterp(0, buff);
    }

    while(1) {
        while( global.connected ==0 ) {
            connectToSpread();
        }
        runFlag=1;

        while(runFlag) {
            bzero(message,sizeof(message));
            bzero(buff,BUFFSIZE);
            ret = SP_receive (global.Mbox, &service_type, sender, 100,
                    &num_groups, target_groups,
                    &mess_type, &endian_mismatch, sizeof (message),
                    message);

            if (ret >= 0) {
                if (Is_regular_mess (service_type)) {
                    fromSpread(sender,message,SINK);
                } else if (Is_membership_mess (service_type)) {
                } else if (Is_reg_memb_mess (service_type)) {
                } else if (Is_caused_join_mess (service_type)) {
                } else if (Is_caused_leave_mess (service_type)) {
                } else if (Is_caused_disconnect_mess (service_type)) {
                } else if (Is_caused_network_mess (service_type)) {
                }

                if(strlen(buff) > 0) {
                    toOut(buff);
                }
            } else {
                SP_error(ret);
                runFlag=0;
                global.connected=0;
            }
        }
    }
    exit(0);
    }
