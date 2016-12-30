#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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
#include "connectToSpread.h"

#define MAX_MESSLEN     102400
#define BUFFSIZE 255

static  char    Spread_name[80];
static    char    Private_group[MAX_GROUP_NAME];

#define MAX_THREAD 100
pthread_mutex_t         stdinMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t         stdoutMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t         stderrMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t         spreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t         cmdInterpMutex = PTHREAD_MUTEX_INITIALIZER;
sem_t connected;

#include "hash.h"
#include "mine.h"

/*! \brief Pass in a pointer to a string, allocate sufficient space to hold it and copy.
*/

char *strsave (char *s) { 
    char *p;

    if ((p = (char *) malloc (strlen (s) + 1)) != NULL) {
        strcpy (p, s);
    }

    return (p);
}

//! \brief Send a message to stderr.

/**
 * @param msg Pointer to a character string.
 */
void printDebug(char *msg) {
    if(global.debug != 0) {
        fprintf(stderr,"%s\n",msg);
    }
}

/**
 * @param msg Pointer to a character string.
 */
void printVerbose(char *msg) {
    if(global.verbose != 0) {
        fprintf(stderr,"%s\n",msg);
    }
}
/*! \brief Convert an integer value to a string presentation.
 * @param v Flag, 0=false, non zero is true.
 * @param res Pointer to a memory block of, at least 6 bytes.
 */
void booleanToString(int v,char *res) {
    if( 0 == v) {
        strcpy(res,"false");
    } else {
        strcpy(res,"true");
    }

}
/*! \brief Display the values held in the globals structure.
*/
void dumpGlobals() {
    char buffer[32];

    booleanToString(global.redisClient,buffer);
    printf("global.redisClient = %s\n",buffer);

    booleanToString(global.rawClient,buffer);
    printf("       rawClient   = %s\n",buffer);

    booleanToString(global.formatClient,buffer);
    printf("       formatClient= %s\n",buffer);

    booleanToString(global.cmdClient,buffer);
    printf("       cmdClient   = %s\n",buffer);

    booleanToString(global.debug,buffer);
    printf("       Debug       = %s\n",buffer);

    booleanToString(global.connected,buffer);
    printf("       Connected   = %s\n",buffer);
}

void connectToSpread() {
    int rc=0;
    int i=0;
    int idx=0;
    int v=0;

    char *tmp;

    char    Private_group[MAX_GROUP_NAME];

    char *spreadServer;
    char *user;
    char *group;
    char *defaultGroup;
    char *c;
    char buffer[BUFFSIZE];
    char *ptr=(char *)NULL;

    if(global.connected) {
        return;
    }

    for(i=0;i<5;i++) {
        bzero(servers[i].server,64);
        servers[i].status=-1;
    }

    idx=0;
    strcpy(servers[idx++].server,"4803@localhost");

    user=getSymbol("USER");
    spreadServer = getSymbol("SPREAD_SERVER");

    if( (char *)NULL != spreadServer ) {

        ptr=strtok(spreadServer,": \n");

        // 
        // Only one server in the list
        //
        if((char *)NULL != ptr) {
            strcpy(servers[idx].server,ptr);
            idx++;
        }

        if( !strcmp(ptr,"4803") || !strcmp(ptr,"4803@localhost")) {
            ptr=(char *)NULL;
        } 

        for(i=idx; (i<5 && ptr != (char *)NULL); i++) {
            ptr=strtok(NULL,": \n");

            if(ptr) {
                strcpy(servers[i].server,ptr);
            }
        }
    }

    idx = 0;
    while( global.connected == 0) {
        if (strlen(servers[idx].server) > 0) {
            rc = SP_connect( servers[idx].server, user, 0, 1, &global.Mbox, Private_group ) ;

            if(rc < 0) {
                if( global.debug) {
                    SP_error(rc);
                    fprintf(stderr,"Failed to contact %s\n", servers[idx].server);
                }

                if ( global.verbose ) {
                    printf( "Failed to connect to spread host %s\n",servers[idx].server);
                }
                global.connected=0;
//                sleep(1);
            } else {
                if ( global.verbose ) {
                    printf( "Connected to spread host %s\n",servers[idx].server);
                }

                global.connected=1;
            }
        }
        idx++;
        idx = idx%4;
    }

    setSymbolValue("ME",Private_group);
    lockSymbol("ME");
    mkLocal("ME");

    v = gethostname(buffer, BUFFSIZE);
    tmp = (char *) strtok(buffer, ".");

    if (tmp) {
        setSymbol("HOSTNAME", tmp, LOCK,LOCAL);
    }


    c=getSymbol("ON_CONNECT");
    if((char *)NULL != c) {
        if( strlen(c) > 0) {
            strcpy(buffer,c);
            strcat(buffer,"\n");

            toOut(buffer);
        }
    }

    group=getSymbol("GROUP");
    if( group != (char *)NULL) {
        if(strlen(group) > 0) {
            rc = SP_join(global.Mbox,group);
        }
    }

    defaultGroup=getSymbol("DEFAULT_GROUP");
    if( defaultGroup != (char *)NULL) {
        if(strlen(defaultGroup) > 0) {
            rc = SP_join(global.Mbox,defaultGroup);
        }
    }

}

void toOut(char *msg ) {
    int rc;

    rc = pthread_mutex_lock(&stdoutMutex);
    fprintf(global.out,"%s",msg);
    fflush(global.out);
    rc = pthread_mutex_unlock(&stdoutMutex);
}

void toError(char *msg ) {
    int rc;

    //    rc = pthread_mutex_lock(&stdoutMutex);
    fprintf(global.out,"%s",msg);
    fflush(global.err);
    //    rc = pthread_mutex_unlock(&stdoutMutex);
}

void fromIn(FILE *fp, char *buffer) {
    int rc=0;
    char *status;

    rc = pthread_mutex_lock(&stdinMutex);

    status = fgets (buffer, BUFFSIZE, global.in);

    rc = pthread_mutex_unlock(&stdinMutex);
}

void getMode(char *resBuffer) {
    char *ptr;
    int resLen;

    ptr= getSymbol("MODE");
    if(!ptr) {
        strcpy(resBuffer,"local");
    } else {
        resLen=strlen(ptr);

        strncpy(resBuffer,ptr,resLen);
        resBuffer[resLen]=0x00;
    }
}

void setMode(char *ptr) {

    setSymbolValue("MODE",ptr);
}

void getGroup(char *resBuffer) {
    int resLen=0;
    char *ptr;

    bzero(resBuffer,BUFFSIZE);
    ptr= getSymbol("GROUP");

    if(strlen(ptr) == 0) {
        ptr= getSymbol("DEFAULT_GROUP");
    }
    resLen=strlen(ptr);
    strncpy(resBuffer,ptr,resLen);
}

void processFormat(char *resBuffer, char *key, char *value, char *fmt) {
    int i=0;
    int j=0;
    int len=0;

    bzero(resBuffer,BUFFSIZE);

    len=strlen(fmt);

    for(i=0;i<len;i++) {
        if( fmt[i] != '$') {
            resBuffer[j++]=fmt[i];
        } else {
            if( fmt[i+1] == 'K') {
                sprintf(&resBuffer[j],"%s",key);
                j += strlen(key);
                i++;
            } else if( fmt[i+1] == 'V') {
                sprintf(&resBuffer[j],"%s",value);
                j += strlen(value);
                i++;
            }
        }
    }
    if(getBoolean("ADD_CR")) {
        strcat(resBuffer,"\n");
    }

}
void remoteSet(char *sender,char *key,char *value,char *resBuffer) {
    char *fmt;
    char *client;

    int len=0;
    int i=0;
    int j=0;
    int flag=0;
    struct nlist *np;

    np=lookup("LAST_MSG_FROM");
    if(!np) {
        install("LAST_MSG_FROM", sender, LOCK, LOCAL);
    } else {
        np->ro = UNLOCK;
        install("LAST_MSG_FROM", sender, LOCK, LOCAL);
        np->ro = LOCK;
    }

    if(global.formatClient != 0 ) {
        fmt=getSymbol("SET_FORMAT");
        processFormat(resBuffer,key,value,fmt);
    } else if(global.redisClient != 0) {
        char scratchBuffer[BUFFSIZE];
        bzero(scratchBuffer,BUFFSIZE);

        redisSender(sender,resBuffer);
        redisSet(key,value,scratchBuffer);
        strcat(resBuffer,scratchBuffer);
    } else if(global.cmdClient !=0) {
        int rc=0;
        char pipeBuffer[BUFFSIZE];
        FILE *pd;

        bzero(pipeBuffer,BUFFSIZE);

        fmt=getSymbol("CMD_FORMAT");

        processFormat(resBuffer,key,value,fmt);

        pd=popen(resBuffer,"r");

        if( pd != (FILE *)NULL) {
            rc=fread(pipeBuffer,1,BUFFSIZE,pd);

            if(strlen(pipeBuffer) > 0) {
                strcpy(resBuffer,pipeBuffer);
            } else {
                bzero(resBuffer,BUFFSIZE);
            }
        }
    }
}
/*
 * No longer called by anything.  Remove ?
 */
/*
 * void remoteGet(char *sender,char *key,char *resBuffer) {
 *    char *client;
 *    char *fmt;
 * 
 *    char scratchBuffer[BUFFSIZE];
 *    int i=0;
 *    int j=0;
 *    int len;
 * 
 *    bzero(scratchBuffer,BUFFSIZE);
 * 
 *    if(global.formatClient != 0) {
 *        bzero(resBuffer,BUFFSIZE);
 * 
 *        fmt=getSymbol("GET_FORMAT");
 * 
 *        len=strlen(fmt);
 *        for(i=0;i<len;i++) {
 *            if( fmt[i] != '$') {
 *                resBuffer[j++]=fmt[i];
 *            } else {
 *                if( fmt[i+1] == 'K') {
 *                    sprintf(&resBuffer[j],"%s",key);
 *                    j += strlen(key);
 *                    i++;
 * 
 *                }
 *            }
 *        }
 *        if(getFiclBoolean("ADD_CR")) {
 *            strcat(resBuffer,"\n");
 *        }
 * 
 *    } else if(global.redisClient != 0) {
 *        redisSender(sender,resBuffer);
 *        redisGet(key,scratchBuffer);
 *        strcat(resBuffer, scratchBuffer);
 *    }
 * }
 */
/*! \brief Generate a redis format 'set' command to set the value of the 'SENDER'.
 * @param [in] sender Sender of spread message.
 * @param [out] res Pointer to buffer holding result.
 */
void redisSender(char *sender, char *res) {
    sprintf(res,"*3\n$3\nSET\n$6\nSENDER\n$%d\n%s\n",strlen(sender),sender);
}

/*! \brief Generate a redis format 'set' command.
 * @param [in] k key
 * @param [in] v value
 * @param [out] buffer Pointer to buffer holding result.
 */
void redisSet(char *k,char *v,char *buffer) {
    sprintf(buffer,"*3\n$3\nSET\n$%d\n%s\n$%d\n%s\n",strlen(k),k,strlen(v),v);
}

/*! \brief Generate a redis format 'hset' command.
 * @param [in] k key
 * @param [in] v value
 * @param [out] buffer Pointer to buffer holding result.
 */
void redisHset(char *k,char *v,char *buffer) {
    sprintf(buffer,"*4\r\n$4\r\nHSET\r\n$%d\r\n%s\r\n$5\r\nVALUE\r\n$%d\r\n%s\r\n",strlen(k),k,strlen(v),v);
}

/*! \brief Generate a redis format 'get' command.
 * @param [in] k key
 * @param [out] buffer Pointer to buffer holding result.
 */
void redisGet(char *k,char *buffer) {
    sprintf(buffer,"*2\n$3\nGET\n$%d\n%s\n",strlen(k),k);
}

/*! \brief Interpret a command recieved from Spread.
 * @param [in] sender The spread user/group the message was recieved from.
 * @param [in] data The message.
 * @param [in] buffer memory to hold the resulting command string.
 */
void remoteCmdInterp(char *sender,char *data,char *buffer) {
    char *cmd;
    char *key;
    char *value;
    char safe[255];

    printDebug("remoteCmdInterp\n");

    if(global.rawClient != 0) {
        return;
    }

    if( data[0] != '^' ) {
        return;
    }

    strcpy(safe,data);

    if(!strncmp(data,"^set ", 5)) {
        printDebug("\tremoteCmdInterp: ^set\n");

        cmd=strtok(data," ");  // split the command line held in data.
        key=strtok(NULL," \n");
        value=strtok(NULL,"\n");
        remoteSet(sender,key,value,buffer);
    } else if(!strncmp(data,"^get ", 5)) {
        printDebug("\tremoteCmdInterp: ^get\n");
        cmd=strtok(data," ");  // split the command line held in data.
        key=strtok(NULL," \n");
        remoteSet(sender,key,value,buffer);
    }

}

/*! \brief Interpret a redis command packet.
 * @param [in] buffer memory to hold the resulting command string.
 * @return 0 if the string was a command, non-zero if it was.
 */
void redisCmdInterp(char *buffer) {
    int tokens=0;
    int len=0;

    int i=0;
    char *tok;
    char buff[BUFFSIZE];

    char ptr[4][32];

    if (tokens == 0) {
        tokens=atoi(&buffer[1]);
    }
    //
    // I am a source.  All I can do is set things.
    //
    // So I need three tokens.  A cmd 'SET' and a key, and a value.
    //
    if( tokens != 3) {
        return;
    }

    bzero(buffer,BUFFSIZE);
    for(i=0;i<tokens;i++) {
        fromIn(global.in,buff);
        if( buff[0] == '$') {
            len=atoi(&buff[1]);
        } else {
            return;
        }
        fromIn(global.in,buff);
        tok=strtok(buff," \r\n");

        if( strlen(tok) != len) {
            return;
        } else {
            strcpy(&ptr[i][0],tok);
        }
    }

    sprintf(buffer,"^%s %s %s\n", &ptr[0][0],&ptr[1][0],&ptr[2][0] );
    toOut("+OK\n");
}
/*! \brief If a string from global.in is a commmand (i.e prefixed by a ^)
 * @param [in] src 
 * @param [in] cmd Pointer to command to interpret.
 * @return 0 if the string was a command, non-zero if it was.
 */
int cmdInterp(int src, char *cmd) {
    char resBuffer[BUFFSIZE];
    char scratch[BUFFSIZE];
    char pipeBuffer[BUFFSIZE];
    FILE *pd;
    int rc=0;

    char *key;
    char *value;
    char *client;
    char *group;

    char *tok;
    char *ptr;
    char *x;

    printDebug("cmdInterp\n");
    //
    // Comments are prefixed with a #
    //
    if (cmd[0] == '#') {
        return(1);
    }
    //
    // The redis command state machine is kicked off by
    // a line that begins with a '*'
    //
    if(cmd[0] == '*') {
        if(global.redisClient != 0) {
            redisCmdInterp(cmd);
            return(0);
        }
    }

    //
    // Local commands are prefixed with a ^
    //
    if(cmd[0] != '^') {
        return(0);
    }

    strcpy(resBuffer,cmd);

    tok=strtok(cmd," \n");

    if( tok !=(char *)NULL) {
        if( !strcmp(cmd,"^mode")) {
            getMode(resBuffer);
            strcat(resBuffer,"\n");
            toOut(resBuffer);
            return(1);
        } else if(!strcmp(cmd,"^status")) {
            if(global.connected !=0) {
                strcpy(resBuffer,"connected\n");
            } else {
                strcpy(resBuffer,"disconnected\n");
            }
            toOut(resBuffer);
            return(1);
        }
        getMode(scratch);
        rc=0;

        if(!strcasecmp(scratch,"local")) {
            if(!strcasecmp(cmd,"^dump")) {
                dumpSymbols();
                dumpGlobals();
                rc=1;
            } else if(!strcasecmp(cmd,"^remote")) {
                if(global.connected == 1) {
                    setMode("remote");
                } else {
                    printf("NOT CONNECTED\n");
                }
                rc=1;
            } else if(!strcasecmp(cmd,"^set-debug")) {
                setSymbolValue("DEBUG","true");
                global.debug=1;
                rc=1;
            } else if(!strcasecmp(cmd,"^clr-debug")) {
                setSymbolValue("DEBUG","false");
                global.debug = 0;
                rc=1;
            } else if(!strcasecmp(cmd,"^connect")) {
                connectToSpread();
                rc=1;
            } else if(!strcasecmp(cmd,"^save")) {
                rc=getBoolean("SAVE_ALLOWED");

                if(rc != 0) {
                    saveSymbols();
                }
                rc=1;
            } else if(!strcasecmp(cmd,"^set")) {
                key=strtok(NULL," ");
                value=strtok(NULL,"\n");
                setSymbol(key,value,UNLOCK,GLOBAL);

                if(!strcmp(key,"CLIENT")) {
                    global.rawClient=0;
                    global.redisClient=0;
                    global.formatClient=0;

                    if(!strcmp(value,"raw")) {
                        global.rawClient=1;
                    }

                    if(!strcmp(value,"redis")) {
                        global.redisClient=1;
                    }

                    if(!strcmp(value,"format")) {
                        global.formatClient=1;
                    }

                    if(!strcmp(value,"cmd")) {
                        global.cmdClient=1;
                    }

                    if( (global.rawClient == 0) && 
                            (global.redisClient == 0) && 
                            (global.formatClient == 0) && 
                            global.cmdClient == 0) {
                        global.rawClient = 1;
                        setSymbolValue("CLIENT","raw");
                    }
                }
                rc=1;
            } else if(!strcasecmp(cmd,"^load")) {
                loadSymbols();
                rc=1;
            } else if(!strcasecmp(cmd,"^lock")) {
                key=strtok(NULL,"\n");
                lockSymbol( key );
                rc=1;
            } else if(!strcasecmp(cmd,"^local")) {
                key=strtok(NULL,"\n");
                mkLocal( key );
                rc=1;
            } else if(!strcasecmp(cmd,"^global")) {
                key=strtok(NULL,"\n");
                mkGlobal( key );
                rc=1;
            } else if(!strcasecmp(cmd,"^exit")) {
                rc=getBoolean("EXIT_ALLOWED");
                if(rc !=0 ) {
                    exit(0);
                }
                rc=1;
            } else if(!strcasecmp(cmd,"^disconnect")) {
                toError("Not Implemented.\n");
            } 
            rc=1;
        }
    } else {
        // Mode is set to remote.
        getGroup(scratch);
        toSpread(scratch,resBuffer);
        rc=1;
    }

    //    rc = pthread_mutex_unlock(&cmdInterpMutex);
    return(rc);
}

/*! \brief If a string from global.in is a commmand (i.e prefixed by a ^)
 * @param [in] arg String from input.
 */
void *count(void *arg) {
    int delay=0;
    int rc;
    char buff[BUFFSIZE];

    while(1) {
        bzero(buff,BUFFSIZE);

        fromIn(global.in,buff);
        // 
        // Check if in remote mode here.
        // 
        if( '^' == buff[0]) {
            rc = pthread_mutex_lock(&cmdInterpMutex);
            cmdInterp(0,buff);
            rc = pthread_mutex_unlock(&cmdInterpMutex);
        } else if( '#' == buff[0]) {
            // Comment so ignore it.
        } else {
            toSpread((char *)"global",(char *)buff);
        }
    }

    return (void*)(1);
}
/*! \brief fromSpread Recieve a message from spread.
 * @param [in] sender the spread user that the message originated with.
 * @param [out] message the message.
 * @param [out] Status.  If < 0 then an error occurred.  If > 0 the message type.
 * 
 */
// TODO make char *m a void *.
//
int fromSpread(char *s, char *m ) {
    char *client;
    char buffer[BUFFSIZE];

    int service_type=0;
    int num_groups;
    char target_groups[100][MAX_GROUP_NAME];
    int16 mess_type;
    int ret;
    int rc=0;
    int rc1=0;

    int endian_mismatch;
    membership_info memb_info;


    char sender[BUFFSIZE];
    static char message[MAX_MESSLEN];

    bzero(buffer,BUFFSIZE);
    bzero(message,MAX_MESSLEN);

    ret = SP_receive (global.Mbox, &service_type, sender, 100,
            &num_groups, target_groups,
            &mess_type, &endian_mismatch, sizeof (message),
            message);

    if( ret < 0) {
        rc = ret;
    } else {
        rc =  service_type;
        if (Is_regular_mess (service_type)) {
            strcpy(s, sender);
            memcpy((void *)m,  message, ret);
        } else if (Is_membership_mess(service_type)) {
            strcpy(s, sender);
            rc1 = SP_get_memb_info(message, service_type, &memb_info);
            strcpy(m,memb_info.changed_member);
        }
    }
    return(rc);
}
/*! \brief toSpread 
 * @param [in] recipient Pointer to the name of the user or group the message is for.
 * @param [in] buffer The message.
 */
// TODO make buffer a void *
//
void toSpread(char *recipient, char *buffer) {
    int rc=-1;
    int ret;
    char *ptr=(char *)NULL;

    rc = pthread_mutex_lock(&spreadMutex);

    if (strlen(buffer) > 0) {
        ret = SP_multicast (global.Mbox, AGREED_MESS, recipient, 1, strlen (buffer), buffer);

        if( ret < 0) {
            SP_error(ret);
            global.connected=0;
        }
    }
    rc = pthread_mutex_unlock(&spreadMutex);
}
/*! \brief loadFile Loads a ficl source file.
 * @param[in] file Filename.
 * @details Reads APPLIB from the environment and, if set, uses this as a prefix to the name of the parameter.  If this is not set or empty it is set to the default  /usr/local/etc/lightToSpread/App.
 */
/*
 * int loadFile(char *file) {
 *    char *env;
 *    char buffer[BUFFSIZE];
 *    char cmd[BUFFSIZE];
 *    int status=0;
 *    int returnValue=0;
 *    char *dir;
 * 
 * #ifdef FICL
 *    struct cString *tmp;
 *    dir=getenv("APPLIB");
 * 
 *    if(!dir) {
 *        tmp=getSymbol("APP_DIR");
 * 
 *        if(!tmp) {
 *            setSymbol("APP_DIR", "/usr/local/etc/lightToSpread/App",UNLOCK,GLOBAL);
 *            dir=strsave("/usr/local/etc/lightToSpread/App");
 *        
 *            fprintf(stderr,"Config variable APPLIB Not set.\n");
 *            fprintf(stderr,"APPLIB set to default: %s\n",dir);
 *        } else {
 *            dir=tmp->text;
 *        }
 *    }
 *    bzero(buffer,BUFFSIZE);
 *    bzero(cmd,BUFFSIZE);
 * //
 * // Check here if file starts with a /
 * // If so do not prepend APPLIB.
 * //
 *    if(file[0] == '/') {
 *        strcpy(buffer,file);
 *    } else {
 *        sprintf(buffer,"%s/%s",dir,file);
 *    }
 * 
 *    if( (access(buffer,R_OK)) != 0 ) {
 *        returnValue=1;
 *    } else {
 *        sprintf(cmd,"load %s\n",buffer);
 *        returnValue = ficlVmEvaluate(global.vm, cmd);
 *        returnValue = 0;
 *    }
 * #endif
 *    return(returnValue);
 * }
 */

/*! \brief Convert a string indicate true/false.
 * @param [in] name Pointer to a string.
 * @return integer value of 0 indicating "false" or 1 indicating "true".
 */

/*
int getBoolean(char *name) {
    char *ptr = getSymbol(name);

    if( ptr) {
        if(!strcmp(ptr,"true")) {
            return(1);
        } else {
            return(0);
        }
    }
}
*/


/*! \brief Test a spread message and return non-zero if I sent it.
 * @param[in] sender Pointer to the senders name.
 * @return -1 if from me, 0 otherwise.
 */
int fromMe(char *sender) {
    char *resPtr;

    resPtr=getSymbol("ME");

    if(!strcmp(sender,resPtr)) {
        return(-1);
    } else {
        return(0);
    }
}

/*! \brief Check if a message is waiting.
 * @param[in] mailbox Mailbox.
 * @return 0 No message waiting.
 * @return <0 Error.
 * @return >0 Number of bytes in message.
 */ 
int spreadPoll() {
    int ret;

    ret = SP_poll( global.Mbox);

    return(ret);
}

/*! \brief Join a group.
 * @param[in] name Pointer to group name.
 */
void spreadJoin(char *group) {

    if(strcmp(group,"global")) {  // Already a memeber og global.
        SP_join(global.Mbox,group);
    }
}


/*! \brief Leave a group.
 * @param[in] name Pointer to group name.
 */
void spreadLeave(char *group) {

    if(strcmp(group,"global")) {  // Can't leave global.
        SP_join(global.Mbox,group);
    }
}

/*! \brief Disconnect from Spread.
*/
void spreadDisconnect() {
    SP_disconnect(global.Mbox);
}

