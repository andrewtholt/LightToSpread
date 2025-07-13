#pragma once

#define MAX_MESSLEN     102400
#define SP_HASHSIZE 31
#define LOCAL 0
#define GLOBAL 1

#define LOCK 0
#define UNLOCK 1

#include <stdlib.h>
#include <pthread.h>

#define BUFFSIZE 255

extern pthread_mutex_t hashLock;
extern int symbolCount;

void lockSymbol(char *);
void mkLocal(char *);
void mkGlobal(char *);
struct nlist *install (char *, void *, int , int );

char *getSymbol(char *);
void setSymbol(char *, const void *, int ,int );
void setSymbolValue(char *, void *);

void setBoolean(char *, int);
char *getFiclParam(char *);
int exists(char *);
void loadSymbols();

char *strsave(char *);

struct cString {
    unsigned char length;
    char text[255];
};


struct nlist {
    char           *name;
    void *def;
    /*
    struct cString *value;
    */

    int             ro;     // 0 if ro 1 if rw
    int             local;  // Some variables I do not want to write on a ^save, so set  this non-zero.

    struct nlist   *next;
};


struct nlist   *lookup(char *);
