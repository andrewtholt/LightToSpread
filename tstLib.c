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

#include "connectToSpread.h"
#include "sp.h"
#include "mine.h"

#define MAX_MESSLEN     102400
#define BUFFSIZE 255


struct globalDefinitions global;

int main(void) {
    global.connected=0;
    global.Group=(char *)NULL;
    global.configFileName=(char *)NULL;
    global.appDir=(char *)NULL;
    global.locked = 0;
    global.rawClient =1;
    global.debug = 1;

    char *group=(char *)NULL;
    
    setSymbol("BUILD",__DATE__);
    setSymbol("CLIENT","raw");
    setSymbol("SPREAD_SERVER","4803");
    setSymbol("GROUP","global");
    setSymbol("USER","tstLib");
    setSymbol("DEBUG","true");
    
    dumpGlobals();
    
    loadSymbols();
    dumpSymbols();
    
    connectToSpread();
    
    printf("============ After\n");
    dumpGlobals();
    
    sleep(10);
}
