
#ifndef SINK
#define SINK 1
#endif

#ifndef SOURCE
#define SOURCE 2
#endif

void toSpread(char *, char *); 
void setFiclParam(char *,char *); 
void dumpSymbols(void);
void saveSymbols(void);
void setFiclParam(char *,char *);
int cmdInterp(int, char *) ;
char *strsave (char *);
int loadFile(char *);
void connectToSpread(void);
void getGroup(char *);
void fromIn(FILE *, char *);
void fromSpread(char *, char *, int);

void toOut(char *);
void toError(char *);
void dumpGlobals();
void printDebug(char *);
void setFiclBoolean(char *,int );


struct globalDefinitions global;

struct spreadServerStatus {
    char server[64];
    int status; // -1 unknown, 0 OK, 1 None fail.
};

struct spreadServerStatus servers[5];


