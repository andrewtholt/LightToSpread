
#ifndef __MINE_H
#define __MINE_H
#include <stdio.h>
#include "sp.h"
#define TRUE 1
#define FALSE 0

void redisSender(char *, char *) ;
void redisGet(char *,char *);
int getFiclBoolean(char *);
void redisSet(char *,char *,char *);
void toOut(char *);

struct globalDefinitions {
    FILE *in;
    FILE *out;
    FILE *err;

    int debug;
    int verbose;

    int rawClient;
    int redisClient;
    int formatClient;
    int cmdClient;
    
    char *defaultGroup;
    int connected;
    //
    // Spread stuff
    //
    mailbox Mbox;
    char *Group;

    char *configFileName;

    int locked;        // If true in remote mode.
    //
    // Ficl stuff.
    //
#ifdef FICL
    char *appDir;      // place to look for ficl files.
    ficlVm *vm;
    ficlSystem *system;
#endif
} ;

#endif
