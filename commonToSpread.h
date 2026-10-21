#ifndef COMMONTOSPREAD_H
#define COMMONTOSPREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include "sp.h"
#include "mine.h"

#define BUFFSIZE 255
#define MAX_MESSLEN 102400
#define SP_HASHSIZE 31

#define CLEAN 0
#define DIRTY 1
#define UNKNOWN 0
#define CONNECTED 1
#define LEAVE 2
#define DISCONNECTED 3

#define RO 1
#define RW 0
#define LOCK 1
#define UNLOCK 0
#define GLOBAL 0
#define LOCAL 1

#define MAX_VSSETS 10
#define MAX_MEMBERS 100

/* Global symbol structure */
struct nlist {
    char *name;
    void *def;
    int ro;     /* 0 if rw, 1 if ro */
    int local;  /* non-zero for local variables not written on ^save */
    struct nlist *next;
};

/* Global variables shared across library and applications */
extern FILE *debugOut;
extern FILE *myStdin;
extern FILE *myStdout;
extern FILE *myStderr;

extern pthread_mutex_t cacheLock;
extern mailbox Mbox;
extern int runFlag;
extern char progName[64];
extern struct globalDefinitions global;

/* String utilities */
char *strsave(const char *s);

/* Symbol table functions */
int hash(char *s);
struct nlist *lookup(char *s);
struct nlist *install(char *name, void *def, int ro, int local);
char *getSymbol(char *name);
void lockSymbol(char *name);
void setSymbol(char *name, void *value, int ro, int local);
void dumpSymbols(void);
void saveSymbols(void);

/* Cache and database functions */
int callback(void *NotUsed, int argc, char **argv, char **azColName);
int lookupCache(char *name);
int lookupCacheGroup(char *name, char *group);
void cacheTouch(char *name);
void cacheClean(void);
void cacheSetConnected(char *name, char *group, int c);
int isConnected(char *name);
void cacheDump(void);
void disconnectCache(void);
void emptyCache(char *all);
void cacheRemove(char *n, char *g);
void cacheAdd(char *n, char *g, int connect);
int execSQL(char *sql);
void setupDatabase(void);

/* Spread network functions */
void spreadDisconnect(void);
int spreadConnect(void);
void spreadLeave(char *group);
int spreadJoin(char *group);
void rxSet(char *name, char *value);
void rxExit(void);
void spreadRX(void);
void startSpreadRX(void);

/* Common lifecycle, configuration, and command processing */
void initDefaultSymbols(const char *progname);
void finishSymbolInitialization(void);
int handleCommonOption(int ch, const char *arg, int *dump_config);
int processConfigFile(const char *filename);
int processSpreadCommand(char *line, int *pRunFlag);

#endif /* COMMONTOSPREAD_H */
