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
#include <stdbool.h>

#include "connectToSpread.h"
#include "hash.h"
#include "sp.h"
#include "mine.h"

#define MAX_MESSLEN     102400
#define BUFFSIZE 255


struct globalDefinitions global;

int main(void) {
    char from[MAX_GROUP_NAME];
    char message[MAX_MESSLEN];
    membership_info memb_info;


    int service_type;
    int rc;
    int ret;

    bool loop=true;

    global.connected=0;
    global.Group=(char *)NULL;
    global.configFileName=(char *)NULL;
    // 
    // Ficl
    //
#ifdef FICL
    global.appDir=(char *)NULL;
#endif
    global.locked = 0;
    global.rawClient =1;
    global.debug = 1;

    char *group=(char *)NULL;

    printf("Buff size = %d\n", MAX_MESSLEN);
    setSymbol("BUILD",__DATE__,LOCK,LOCAL);
    setSymbol("CLIENT","raw",UNLOCK,GLOBAL);
    setSymbol("SPREAD_SERVER","4803@192.168.10.124",UNLOCK,GLOBAL);
    setSymbol("GROUP","global",UNLOCK,GLOBAL);
    setSymbol("USER","tstLib",UNLOCK,GLOBAL);
    setSymbol("DEBUG","true",UNLOCK,GLOBAL);
    setSymbol("MODE","local",UNLOCK,GLOBAL);

    dumpGlobals();

    loadSymbols();
    loadFile("tst.rc");
    dumpSymbols();

    connectToSpread();

    rc = spreadPoll();

    dumpGlobals();
    while(loop) {
        memset(message,0,128);
        service_type = fromSpread(from,message);

        if (Is_regular_mess (service_type)) {
            printf("Message\n");

            printf("Sender %s\n",from);
            printf("Message >%s<\n",message);
            printf("Message size >%d<\n",(int)sizeof(message));

            toSpread(from,"Message recieved\n");
            //      loop=false;
        } else if (Is_membership_mess (service_type)) {
            printf("Membership\n");
            // TODO segv here.
            ret = SP_get_memb_info(message, service_type, &memb_info);

        } else if (Is_reg_memb_mess (service_type)) {
            printf("Regular memebership\n");
        } else if (Is_caused_join_mess (service_type)) {
            printf("Join\n");
        } else if (Is_caused_leave_mess (service_type)) {
            printf("Leave\n");
        } else if (Is_caused_disconnect_mess (service_type)) {
            printf("Disconnect\n");
        } else if (Is_caused_network_mess (service_type)) {
            printf("Network\n");
        }
    }


    printf("============ After\n");
    dumpGlobals();

    printf("Sender %s\n",from);
    printf("Message >%s<\n",message);

    toSpread(from,"Message recieved\n");

    spreadJoin("TEST");

    sleep(2);

    spreadLeave("TEST");

    sleep(2);

    spreadDisconnect();
}
