
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
    int rawClient;
    int redisClient;
    int formatClient;
//    int ficlClient;
    int cmdClient;
    
    char *defaultGroup;
    int connected;
    //
    // Spread stuff
    //
    mailbox Mbox;
    char *Group;

    char *configFileName;

    char *appDir;      // place to look for ficl files.
    int locked;        // If true in remote mode.
    //
    // Ficl stuff.
    //
#ifdef FICL
    ficlVm *vm;
    ficlSystem *system;
#endif
} ;
