#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ficl.h>
#include <semaphore.h>

#include <termios.h>
#include "mine.h"

struct termios orig_termios; 
char prompt[32];
char *loadPath;

#ifdef MAC
#include <sys/sysctl.h>
#endif

#include "sp.h"

#define MAX_MESSLEN     102400
#define BUFFSIZE 255

static  char    Spread_name[80];
static    char    Private_group[MAX_GROUP_NAME];

#define MAX_THREAD 100
// pthread_mutex_t         stdinMutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t         stdoutMutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t         stderrMutex = PTHREAD_MUTEX_INITIALIZER;

// pthread_mutex_t         spreadMutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t         cmdInterpMutex = PTHREAD_MUTEX_INITIALIZER;
sem_t connected;

struct globalDefinitions global;

void usage(void) {
    printf("Usage:\n\n");
    printf("\t-h\tHelp\n");
}


int main(int argc, char* argv[]) {
    pthread_t *threads;
//    param *p;
    int rc=0;

    int service_type=0;
    char sender[MAX_GROUP_NAME];
    int num_groups;

    char target_groups[100][MAX_GROUP_NAME];
    int16 mess_type;
    static char message[MAX_MESSLEN];
    int endian_mismatch;
    int ret;
    int ch;
    char *env=(char *)NULL;
    char *ptr;

    int returnValue=0;
    int loopFlag=1;

    char cmdBuffer[BUFFSIZE];
//    struct cString *tmp;
    char *tmp;

    global.connected=0;
    global.Group=(char *)NULL;
    global.configFileName=(char *)NULL;
    global.appDir=(char *)NULL;
    global.locked = 0;

    global.debug = 1;
    int runFlag=1;
    int semvalue=0;
    char *user=(char *)NULL;
    char *debug=(char *)NULL;
    char *interactive=(char *)NULL;
    char *group=(char *)NULL;
    char *spreadServer=(char *)NULL;

    env=getenv("APPLIB");

    if((char *)NULL == env) {
        fprintf(stderr,"APPLIB not set, goodbye.\n");
        exit(-1);
    }
//    global.appDir = (char *)strsave(env);
//    global.defaultGroup = strsave("global");

    global.system = ficlSystemCreate(NULL);
    ficlSystemCompileExtras(global.system);
    global.vm = ficlSystemCreateVm(global.system);
    ficlSystemCompileExtras(global.system);

    setFiclParam("INTERACTIVE","false");
    setFiclParam("MODE","local");
    setFiclParam("BUILD",__DATE__);
    mkLocal("BUILD");
    lockSymbol("BUILD");
    setFiclParam("DEBUG","false");
    setFiclParam("SPREAD_SERVER","4803");
    setFiclParam("START_FILE","./start.rc");
    setFiclParam("CLIENT","raw");

    setFiclParam("SET_FORMAT","$K $V");
    setFiclParam("GET_FORMAT","$K");
    setFiclParam("ADD_CR","true");

    setFiclParam("USER","lite");
    setFiclParam("SAVE_ALLOWED","true");
    setFiclParam("AUTOCONNECT","false");
    setFiclParam("TO_FORTH","false");
    setFiclParam("ON_CONNECT","connected");
    setFiclParam("EXIT_ALLOWED","true");
    setFiclParam("INTERACTIVE","false");

    while ((ch = getopt (argc, (char **) argv, "c:dhig:u:s:")) != -1) {
        switch(ch) {
            case 'c':
                setFiclParam("CFG_FILE",optarg);
                break;
            case 'd':
                debug=strsave("true");
                break;
            case 'h':
                usage();
                exit(0);
                break;
            case 'i':
                interactive=strsave("true");
                break;
            case 'g':
                group = strsave( optarg );
                break;
            case 'u':
                user=strsave(optarg);
                break;
            case 's':
                spreadServer=strsave(optarg);
                break;
        }
    }

    threads=(pthread_t *)malloc(sizeof(*threads));

    rc = sem_init(&connected,0,0);
    if( rc != 0) {
        perror("Semaphore faulure;");
        exit(-10);
    }

// p=(param *)malloc(sizeof(param) );
//    p->delay=3;

    /*
     * Read config file here.
     */
    loadSymbols();
    dumpSymbols();

    setFiclParam("DEFAULT_GROUP",global.defaultGroup);
    setFiclParam("APP_DIR",global.appDir);

    ptr = getFiclParam("CLASS");
    if(!ptr) {
        setFiclParam("CLASS","client");
    }
    if( user != (char *)NULL) {
        setFiclParam("USER",user);
    }

    if( debug !=(char *)NULL) {
        setFiclParam("DEBUG","true");
    }

    if( interactive !=(char *)NULL) {
        if(!strcmp(interactive,"true")) {
            setFiclParam("INTERACTIVE","true");
        } else {
            setFiclParam("INTERACTIVE","false");
        }
    }

    if( group !=(char *)NULL) {
        setFiclParam("GROUP",group);
        free(group);
    } else {
        setFiclParam("GROUP","NOT-SET");
    }

    if( spreadServer !=(char *)NULL) {
        setFiclParam("SPREAD_SERVER",spreadServer);
    }

    global.in = fopen("/dev/tty","r");
    global.out = fopen("/dev/tty","w");
    global.err = fopen("/dev/tty","w");

    if(loadFile("lib.fth") != 0) {
        fprintf(stderr,"lib.fth: Failed to load file.\n");
        exit(-3);
    }

    if(loadFile("classes.fth") != 0) {
        fprintf(stderr,"app_cfg.fth: Failed to load file.\n");
        exit(-3);
    }

    sprintf(cmdBuffer,"only forth also oop definitions seal\n");
    returnValue = ficlVmEvaluate(global.vm, cmdBuffer);

    if(loadFile("app_cfg.fth") != 0) {
        fprintf(stderr,"app_cfg.fth: Failed to load file.\n");
        exit(-3);
    }


    loadFile("local_cfgapp.fth");

    //    rc = sem_getvalue(&connected, &semvalue);
    //    printf("semvalue=%d\n",semvalue);

    //    setSymbol((char *)"autoconnect", (char *)"true", UNLOCK,GLOBAL);
    //    tmp=getSymbol("AUTOCONNECT");

    //    ficlStackPushPointer(global.vm->dataStack, (void *)tmp);
    //    returnValue = ficlVmEvaluate(global.vm, cmdBuffer);

    loopFlag=getFiclBoolean("INTERACTIVE");

    while(loopFlag !=0) {
        printf("OK>");
        fflush(stdout);
        ptr=fgets(cmdBuffer,BUFFSIZE,stdin);
        returnValue = ficlVmEvaluate(global.vm, cmdBuffer);

        tmp=getFiclParam("INTERACTIVE");

        if(!strcmp(tmp,"false")) {
            loopFlag=0;
        }

    }

    rc=pthread_create(&threads[0],NULL,NULL,NULL );

    if(rc != 0) {
        printf("Failed to create thread, bye\n\n");
        exit(1);
    }
    if( getFiclBoolean("AUTOCONNECT") ) {
        sem_post(&connected);
        //        connectToSpread();
    }

    rc = sem_getvalue(&connected, &semvalue);
    printf("semvalue=%d\n",semvalue);
    rc=sem_wait(&connected);

    while(1) {
        rc = sem_getvalue(&connected, &semvalue);
        printf("semvalue=%d\n",semvalue);

        connectToSpread();

        runFlag=1;

        rc = SP_join(global.Mbox,global.defaultGroup);

        getGroup(cmdBuffer);

        if(strlen(cmdBuffer) > 0) {
            rc = SP_join(global.Mbox,cmdBuffer);
        }

        if(rc < 0) {
            SP_error(rc);
            exit(1);
        }

        while(runFlag) {
            bzero(message,sizeof(message));

            ret = SP_receive (global.Mbox, &service_type, sender, 100,
                    &num_groups, target_groups,
                    &mess_type, &endian_mismatch, sizeof (message),
                    message);

            if(ret<0) {
                fprintf(stderr,"SP_recieve %d;",ret);
                SP_error(rc);

                runFlag=0;
            }

            if(!fromMe(sender)) {
                char *ptr;
                
                ptr=getFiclParam("CLASS");
                
                if (Is_regular_mess (service_type)) {
                    if( !strcmp(ptr,"client") || !strcmp(ptr,"both") ) {
                        fromSpread(sender,message);
                    }
                } else if (Is_membership_mess (service_type)) {
                } else if (Is_reg_memb_mess (service_type)) {
                } else if (Is_caused_join_mess (service_type)) {
                } else if (Is_caused_leave_mess (service_type)) {
                } else if (Is_caused_disconnect_mess (service_type)) {
                } else if (Is_caused_network_mess (service_type)) {
                } 
            }
        }
        global.connected=0;
    }

//    free(p);

    exit(0);
}
