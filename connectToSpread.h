#include <stdbool.h>

#ifndef SINK
#define SINK 1
#endif

#ifndef SOURCE
#define SOURCE 2
#endif

int loadFile(char *);
void dumpSymbols(void);
void saveSymbols(void);

int cmdInterp(int, char *) ;
char *strsave (char *);
void getGroup(char *);

int fromSpread(char *, char *);
void toSpread(char *, char *);
  
void setBoolean(char *, int);

void toOut(char *);
void toError(char *);
void fromIn(char *);

void dumpGlobals();
void printDebug(char *);

void connectToSpread(void);
void spreadJoin(char *);
void spreadLeave(char *);
void spreadDisconnect(void);
int spreadPoll();

int getBoolean(char *v);

void redisHset(char *k,char *v,char *buffer);

extern struct globalDefinitions global;

struct spreadServerStatus {
    char server[64];
    int status; // -1 unknown, 0 OK, 1 None fail.
};

struct spreadServerStatus servers[5];


