/*
 * 
 * Date: Thu 26 Feb 2009
 * 
 * ToDo
 * 
 * 1. in spreadRX catch join messages and if unknown create entries in cache marked dirty & connected.  If known mark as connected.
 * 2.             catch leave & exit messages and if unknown create entries in cache marked dirty & not connected. If know mark as disconnected.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>		/* pthread functions and data structures */
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <sqlite3.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pwd.h>

#include "sp.h"
#define MAX_MESSLEN     102400
mailbox         Mbox;

#define BUFFSIZE 255
char           *getSymbol(char *name);
void            setSymbol(char *name, void *value, int string,int local);
#define CLEAN 0
#define DIRTY 1
#define UNKNOWN 0
#define DISCONNECTED 1
#define CONNECTED 2
#define LEAVE 3

#define LOCK 0
#define UNLOCK 1

#define HASHSIZE 31
#define LOCAL 0
#define GLOBAL 1
#define MAX_VSSETS      10
#define MAX_MEMBERS 100

FILE *debugOut;
FILE *myStdin;
FILE *myStdout;
FILE *myStderr;

int             runFlag = 1;
pthread_mutex_t hashLock;
pthread_mutex_t cacheLock;

/*
 *   static struct cache *head = NULL;
 * 
 *   struct cache {
 *   char           *name;
 *   char           *group;
 *   int             dirty;
 *   int             connected;
 *   struct cache   *next;
 *   };
 * 
 *   static struct cache cacheHash[HASHSIZE];
 */

struct nlist {
    char           *name;
    void           *def;

    //	int             string; // 0 if not a string
    int             ro;     // 0 if rw 1 if ro
    int             local;  // Some variables I do not want to write on a ^save, so set  this non-zero.
    struct nlist   *next;
};

    char           *
strsave(char *s)
{
    char           *p;

    if ((p = malloc(strlen(s) + 1)) != NULL)
        strcpy(p, s);

    return (p);
}

#define READ 0
#define WRITE 1
pid_t popen2(const char *command, int *infp, int *outfp)
{
    int p_stdin[2], p_stdout[2];
    pid_t pid;

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        return -1;

    pid = fork();

    if (pid < 0)
        return pid;
    else if (pid == 0)
    {
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], WRITE);

        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(1);
    }

    if (infp == NULL)
        close(p_stdin[WRITE]);
    else
        *infp = p_stdin[WRITE];

    if (outfp == NULL)
        close(p_stdout[READ]);
    else
        *outfp = p_stdout[READ];

    return pid;
}

void usage() {
    printf("usage: toSpread -h -c <cfg> -u <user> -d <database> -f -n <hostname> -p <template>\n\n");

    printf("\t-c <cfg>\tConfig file.\n");
    printf("\t-d <database>\tCache databae.\n");
    printf("\t-f\t\tForce, remove default config.\n");
    printf("\t-h\t\tHelp.\n");
    printf("\t-n <hostname>\tSet hostname.\n");
    printf("\t-p <template>\tPipe name template.  See NOTES.\n");
    printf("\t-u <user>\tSpread user.\n");

    printf("\nNOTES:\n\n");
    printf("\tDefaults are:\n");
    printf("\t\ttoSpread -u <unix user> -g global -c ~/.start.rc\n\n");

    printf("\tThe -p option creates two named pipes (aka fifo's) using the given\n\ttemplate.\n\n");
    printf("\t-p /tmp/fred would cause two fifo's to be created\n\n");
    printf("\te.g.\t/tmp/fred.0 - stdin\n");
    printf("\t\t/tmp/fred.1 - stdout\n\n");
    printf("\tSo a client would write to fred.0 and read from fred.1.\n");

}

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    NotUsed=0;
    int i;
    printf("CALLBACK\n");
    for(i=0; i<argc; i++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i]: "NULL");
    }
    printf("\n");
    return 0;
}

int lookupCache(char *name)
{
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows,columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"select count(*) from nodes where name='%s';",name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut,"lookupCacheGroup fatal DB error.\n");
        fflush(debugOut);
        exit(-1);
    }
    cnt=atoi(results[columns]);

    sqlite3_close(mydb);
    return(cnt);

}

int lookupCacheGroup(char *name, char *group)
{
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows,columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"select count(*) from nodes where sp_group='%s' and name='%s';",group, name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut,"lookupCaceGroup fatal DB error.\n");
        fflush(debugOut);
        exit(-1);
    }
    cnt=atoi(results[columns]);

    sqlite3_close(mydb);
    return(cnt);

}

    void
cacheTouch(char *name)
{

    sqlite3 *mydb;

    char *dbName;
    char sql[BUFFSIZE];
    char *zErrMsg;
    int rc;
    time_t now;
    now = time(NULL);

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql , "update nodes set touched=%d; where name='%s';",(int)now,name);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"cacheTouch:INSERT:%s\n",zErrMsg);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fflush(debugOut);
    }

    sqlite3_close(mydb);

}

void cacheClean()
{
    sqlite3 *mydb;

    char *dbName;
    char sql[BUFFSIZE];
    char *zErrMsg;
    //    char *debug;
    int rc;

    time_t now;
    time_t stale;

    now = time(NULL);

    char *purgeAfter;


    dbName = getSymbol("DATABASE");
    purgeAfter = getSymbol("STALE_AFTER");

    stale=now+atoi(purgeAfter);

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"delete from nodes where state <> 'CONNECTED' and stale < %d;",(int)now);
    //    printf("%s\n",sql);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"cacheClean:INSERT:%s\n",zErrMsg);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fflush(debugOut);
    }

    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}


void cacheSetUnknown( char *group) {
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc=0;
    char *zErrMsg;

    dbName = getSymbol("DATABASE");

    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"update nodes set state='UNKNOWN' where  sp_group='%s';", group);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);

    sqlite3_close( mydb );
}

void cacheSetConnected(char *name, char *group, int c) {
    char *dbName;
    int rc=0;
    char sql[BUFFSIZE];
    char tmp[16];
    char *zErrMsg;
    char *t1;

    sqlite3 *mydb;
    time_t now,stale;

    now = time(NULL);

    dbName = getSymbol("DATABASE");
    t1=(char *)getSymbol("STALE_AFTER");

    stale = now + atoi(t1);
    rc = sqlite3_open(dbName, &mydb);

    switch(c) {
        case CONNECTED:
            strcpy(tmp,"CONNECTED");
            break;
        case DISCONNECTED:
            strcpy(tmp,"DISCONNECTED");
            break;
        case LEAVE:
            strcpy(tmp,"LEAVE");
            break;
        default:
            strcpy(tmp,"UNKNOWN");
            break;
    }

    sprintf(sql,"update nodes set state='%s',touched=%d,stale=%d where name='%s' and sp_group='%s';",tmp,(int)now,(int)stale,name,group);
    //    sprintf(sql,"update nodes set state='%s' where name='%s' and sp_group='%s';",tmp,name,group);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"cacheSetConnected:update:%s\n",zErrMsg);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fflush(debugOut);
    }

    sqlite3_close( mydb );
}

    int
inCache(char *name)
{
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows,columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"select count(*) from nodes where name='%s';",name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut,"lookupCaceGroup fatal DB error.\n");
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fflush(debugOut);
        exit(-1);
    }
    cnt=atoi(results[columns]);

    sqlite3_close(mydb);
    return(cnt);
}

    int
isConnected(char *name)
{
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows,columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"select count(*) from nodes where name='%s' and state = 'CONNECTED';",name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut,"lookupCaceGroup fatal DB error.\n");
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fflush(debugOut);
        exit(-1);
    }
    cnt=atoi(results[columns]);

    sqlite3_close(mydb);
    return(cnt);
}

void cacheDump() {
    sqlite3 *mydb;

    char *dbName;
    char output[BUFFSIZE];
    char *sql;
    char *zErrMsg;
    int rc;
    char **results = NULL;
    int rows,columns;
    int i;
    int j;
    //    struct tm date;
    time_t clock;
    char *tmp;
    int interactive=0;

    tmp=getSymbol("INTERACTIVE");

    if (!tmp) {
        interactive=0;
    } else {
        interactive = (strcmp(tmp,"false") !=0);
    }

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);
    strcpy(output,"|");

    sql="select name, sp_group, state,touched from nodes order by name,sp_group,touched;";

    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK) {
        fprintf(debugOut,"DBERROR:cacheDump:%s\n",zErrMsg);
        fflush(debugOut);
    } else {
        if(interactive) 
        {
            fprintf(myStdout,"================================================================================\n");
            fprintf(myStdout,"|Name                     |Group          |State       |Touched\n");
            fprintf(myStdout,"================================================================================\n");
        } else {
            fprintf(myStdout,"START_CACHE\n");
        }
        for(i = 1; i <= rows; i++) {
            for(j=0;j<columns;j++) {
                switch(j) {
                    case 0:
                        if(interactive) {
                            fprintf(myStdout,"|%-25s|",results[(i*columns)+j]);
                        } else {
                            fprintf(myStdout,"%s:",results[(i*columns)+j]);
                        }
                        break;
                    case 1:
                        if(interactive) {
                            fprintf(myStdout,"%-15s|",results[(i*columns)+j]);
                        } else {
                            fprintf(myStdout,"%-s:",results[(i*columns)+j]);
                        }
                        break;
                    case 2:
                        if(interactive) {
                            fprintf(myStdout,"%-12s|",results[(i*columns)+j]);   
                        } else {
                            fprintf(myStdout,"%s:",results[(i*columns)+j]);   
                        }
                        break;
                    case 3:
                        clock = (time_t) atoi(results[(i*columns)+j]);
                        if(interactive) {
                            fprintf(myStdout,"%s",asctime(localtime( &clock ) ) );
                        } else {
                            fprintf(myStdout,"%d:\n",(int)clock);
                        }
                        break;
                    default:
                        fprintf(myStdout,"%-5s|",results[(i*columns)+j]);
                        break;
                }
            }
            strcpy(output,"|");
        }
        if(interactive)
        {
            fprintf(myStdout,"================================================================================\n");
        } else {
            fprintf(myStdout,"END_CACHE\n");
        }
    }
    fflush(myStdout);

}

void disconnectCache() {
    char *me;
    int rc;

    char *dbName;
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *zErrMsg;

    sprintf(sql,"update nodes set state = 'DISCONNECTED';");

    dbName = getSymbol("DATABASE");

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"disconnectCache:UPDATE:%s\n",zErrMsg);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fprintf(debugOut,"%s\n",sql);
        fflush(debugOut);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}
void emptyCache(char *all) {
    sqlite3 *mydb;
    char *dbName;
    char *me;

    int rc;
    char *zErrMsg;
    char sql[BUFFSIZE];

    time_t now;

    now = time(NULL);
    me = getSymbol("ME");
    dbName = getSymbol("DATABASE");

    pthread_mutex_lock(&cacheLock);

    rc = sqlite3_open(dbName, &mydb);

    if (all == (char *)NULL) {
        sprintf(sql,"delete from nodes where state = 'DISCONNECTED' and name <> '%s';",me);
    } else if(!strcmp(all,"all")) {
        sprintf(sql,"delete from nodes where state <> 'CONNECTED';");

    } else if(!strcmp(all,"stale")) {
        sprintf(sql,"delete from nodes where state <> 'CONNECTED' and stale < %d;",(int)now);
    }

    //    printf("%s\n",sql);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"cacheTouch:INSERT:%s\n",zErrMsg);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fprintf(debugOut,"%s\n",sql);
        fflush(debugOut);
    }

    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);

}

    void
cacheSave()
{

}

/*
 *   void
 *   cacheTouchAll()
 *   {
 *   sqlite3 *mydb;
 * 
 *   char *dbName;
 *   char sql[BUFFSIZE];
 *   char *zErrMsg;
 *   int rc;
 *   time_t now;
 *   now = time(NULL);
 * 
 *   dbName = getSymbol("DATABASE");
 *   rc = sqlite3_open(dbName, &mydb);
 * 
 * 
 *   sprintf(sql , "update nodes set dirty='Y',touched=%d;",now);
 * 
 *   rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
 *   if(rc != SQLITE_OK ) {
 *   fprintf(debugOut,"cacheTouchAll:INSERT:%s\n",zErrMsg);
 *   fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
 *   fflush(debugOut);
 *   }
 * 
 *   sqlite3_close(mydb);
 * 
 *   }
 */
void cacheRemove(char *n, char *g) {
    char *dbName;
    char sql[BUFFSIZE];
    char scratch[BUFFSIZE];
    char *zErrMsg;

    sqlite3 *mydb;
    int rc;

    dbName = getSymbol("DATABASE");
    pthread_mutex_lock(&cacheLock);

    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql,"delete from nodes where name='%s'",n);

    if(g) {
        sprintf(scratch," and sp_group='%s';",g);
    } else {
        strcpy(scratch,";");
    }
    strcat(sql,scratch);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"DBERROR:cacheDelete:%s\n",zErrMsg);
        fflush(debugOut);
    }

    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

// void cacheAdd(char *n, char *g, int dirty, int connect) {
void cacheAdd(char *n, char *g, int connect) {

    int rc=0;
    int cnt=0;

    sqlite3 *mydb;

    char *dbName;
    char sql[BUFFSIZE];
    char status[32];

    char **results = NULL;
    int rows,columns;
    char *zErrMsg;
    time_t now;
    now = time(NULL);

    dbName = getSymbol("DATABASE");

    pthread_mutex_lock(&cacheLock);

    rc = sqlite3_open(dbName, &mydb);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);

    sprintf(sql,"select count(*) from nodes where name='%s' and sp_group='%s';",n,g);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut,"DBERROR:%s\n",zErrMsg);
        fflush(debugOut);
    }
    cnt=atoi(results[columns]);

    switch(connect) {
        case CONNECTED:
            strcpy(status,"CONNECTED");
            break;
        case DISCONNECTED:
            strcpy(status,"DISCONNECTED");
            break;
        case LEAVE:
            strcpy(status,"LEAVE");
            break;
        default:
            strcpy(status,"UNKNOWN");
            break;
    }

    if (cnt == 0) {
        sprintf(sql,"insert into nodes ( sp_group,name,state,touched ) values('%s','%s','%s',%d);",g,n,status,(int)now);
    } else {
        sprintf(sql,"update nodes set state='%s',touched=%d where name='%s' and sp_group='%s';",status,(int)now,n,g);
    }

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"cacheAdd:INSERT:%s\n",zErrMsg);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(mydb));
        fflush(debugOut);
    }

    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

int execSQL( char *sql ) {
    char *zErrMsg;
    int rc;
    sqlite3 *mydb;
    char *dbName;

    dbName = getSymbol("DATABASE");
    pthread_mutex_lock(&cacheLock);

    rc = sqlite3_open(dbName, &mydb);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);

    sqlite3_close(mydb);
    pthread_mutex_lock(&cacheLock);
    return(rc);
}

void cacheLoad(char *file) {
    FILE           *fp;
    char            buffer[BUFFSIZE];
    char           *ret;
    char           *name;
    char           *group;

    fp = fopen(file, "r");

    if (fp) {
        while ((ret = fgets(buffer, BUFFSIZE, fp)) != 0) {
            buffer[strlen(buffer) - 1] = 0x0;
            name = (char *) strtok(buffer, ":");
            group = (char *) strtok(NULL, "\n");
            cacheAdd(name, group, DISCONNECTED);
        }
    }
}

static struct nlist *hashtab[HASHSIZE];

void dumpSymbols() {
    struct nlist   *np;
    int             i;
    char            fmt[BUFFSIZE];
    char *tmp;

    int interactive=0;

    tmp=getSymbol("INTERACTIVE");

    if (!tmp) {
        interactive=0;
    } else {
        interactive = (strcmp(tmp,"false") !=0);
    }

    if( interactive) {
        fprintf(myStdout,"==============================================================|\n");
        fprintf(myStdout,"|\tBuild Date:%s\n",getSymbol("BUILD"));
        fprintf(myStdout,"==============================================================|\n");

        strcpy(fmt, "|%-20s|%-25s|%-6s|%-7s|\n");
        fprintf(myStdout,fmt, "Name", "Value", "Locked","Scope");
        fprintf(myStdout,"==============================================================|\n");
    } else {
        fprintf(myStdout,"START_SYMBOLS\n");
    }
    for (i = 0; i < HASHSIZE; i++) {
        if (hashtab[i] != 0) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if(interactive) {
                    fprintf(myStdout,"|%-20s", np->name);
                    fprintf(myStdout,"|%-25s", (char *) np->def);

                    if (np->ro == LOCK)
                        fprintf(myStdout,"|%-6s|", "Yes");
                    else
                        fprintf(myStdout,"|%-6s|", "No");

                    if (np->local == LOCAL)
                        fprintf(myStdout,"%-7s|","Local");
                    else
                        fprintf(myStdout,"%-7s|","Global");
                } else {
                    fprintf(myStdout,"%s:", np->name);
                    fprintf(myStdout,"%s:", (char *) np->def);

                    if (np->ro == LOCK)
                        fprintf(myStdout,"%s:", "LOCKED");
                    else
                        fprintf(myStdout,"%s:", "UNLOCKED");

                    if (np->local == LOCAL)
                        fprintf(myStdout,"%s:","LOCAL");
                    else
                        fprintf(myStdout,"%s:","GLOBAL");
                }
                fprintf(myStdout,"\n");
            }
        }
    }

    if(interactive) {
        fprintf(myStdout,"==============================================================|\n");
    } else {
        fprintf(myStdout,"END_SYMBOLS\n");
    }
    fflush(myStdout);
}

void saveSymbols() {
    struct nlist   *np;
    int             i;
    char           *fileName;
    char           *save;
    FILE           *fp;
    time_t now;

    save = getSymbol("SAVE_ALLOWED");

    if (save) {
        if(!strcmp(save,"false")) {
            return;
        }
    }

    fileName = getSymbol("START_FILE");
    if (!fileName) {
        setSymbol("START_FILE", "./start.rc", UNLOCK,LOCAL);
        fileName = getSymbol("START_FILE");
    }
    fp = fopen(fileName, "w");

    if (!fp)
        return;

    now = time(NULL);

    fprintf(fp,"#\n");
    fprintf(fp,"# Save Date:%s",asctime(localtime( &now )));
    fprintf(fp,"#\n");
    fprintf(fp,"# toSpread startup file.\n");
    fprintf(fp,"# Comments MUST be at the start of a line.\n");
    fprintf(fp,"# Any comments added will be lost.\n");    
    fprintf(fp,"#\n");
    for (i = 0; i < HASHSIZE; i++) {
        if (hashtab[i] != 0) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if(np->local == GLOBAL) {
                    fprintf(fp, "^set %s ", np->name);
                    fprintf(fp, "%s", (char *) np->def);

                    fprintf(fp, "\n");

                    if (np->ro == LOCK) {
                        fprintf(fp, "^lock %s\n", np->name);
                    }
                }
            }
        }
    }


    fclose(fp);

}

int hash(char *s) {
    int             hashval;

    for (hashval = 0; *s != '\0';)
        hashval += *s++;
    return (hashval % HASHSIZE);
}

struct nlist *lookup(char *s) {
    struct nlist   *np;

    for (np = hashtab[hash(s)]; np != NULL; np = np->next)
        if (!strcmp(s, np->name))
            return (np);
    return ((struct nlist *) NULL);
}

struct nlist   *install(char *name, void *def, int ro,int local) {
    struct nlist   *np;
    int             hashval;

    if ((np = lookup(name)) == NULL) {
        np = (struct nlist *) malloc(sizeof(*np));
        if (np == NULL)
            return (NULL);

        if ((np->name = strsave(name)) == NULL)
            return (NULL);

        hashval = hash(np->name);
        np->next = hashtab[hashval];
        hashtab[hashval] = np;
        np->ro = ro;
        np->local=local;
        np->def = (char *) strsave(def);
    } else {
        if (np->ro == UNLOCK) {
            free(np->def);
            np->def = strsave(def);
        }
    }
    return (np);
}

char *getSymbol(char *name) {
    char           *p;
    struct nlist   *r;
    pthread_mutex_lock(&hashLock);
    r = lookup(name);

    if (r) {
        p = r->def;
    } else {
        p = (char *) NULL;
    }

    pthread_mutex_unlock(&hashLock);
    return (p);
}


    void
lockSymbol(char *name)
{
    struct nlist   *np;

    np = lookup(name);

    if (np)
        np->ro = LOCK;
}

void setSymbol(char *name, void *value, int ro,int local) {
    pthread_mutex_lock(&hashLock);
    (void) install(name, value, ro, local);
    pthread_mutex_unlock(&hashLock);
}

void
spreadDisconnect() {

    char sql[BUFFSIZE];
    sqlite3 *db;
    char *dataFile;
    char *zErrMsg;
    int rc;
    char *connected;

    SP_disconnect(Mbox);
    runFlag=0;
    sleep(1); // block, and wait for thread to exit
    connected=getSymbol("CONNECTED"); // get CONNECTED, if still set add loop

    dataFile =getSymbol("DATABASE");
    rc = sqlite3_open(dataFile, &db);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"spreadDisconnect:ERROR:%s\n", sql);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(db));
        fflush(debugOut);
    } 

    strcpy(sql,"update nodes set state='DISCONNECTED';");
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);

    if(rc != SQLITE_OK) {
        fprintf(debugOut,"spreadDisconnect:ERROR:%s\n", sql);
        fprintf(debugOut,"%s\n",sqlite3_errmsg(db));
        fflush(debugOut);
    } 

    sqlite3_close(db);
}

int spreadConnect() {
    int             ret;
    char           *debug;
    char scratch[255];

    char *server=(char *)NULL;
    char           *mainServer;
    char           *altServer;
    char           *user;
    char           *group;

    char *tmp;
    char *action;
    int dbg=0;

    char            Private_group[MAX_GROUP_NAME];

    setSymbol("CONNECTED", "false",UNLOCK,LOCAL);

    disconnectCache();
    //make sure that all the required variables are set

    debug = (char *)getSymbol("DEBUG");

    dbg = ( !strcmp(debug,"true") );

    mainServer = (char *) getSymbol("SERVER");
    altServer = (char *) getSymbol("ALTSERVER");

    if(altServer == (char *)NULL) {
        altServer = mainServer;
    }

    user = (char *) getSymbol("USER");
    group = (char *) getSymbol("GROUP");

    if(dbg) {
        fprintf( debugOut, "Connecting to %s ... \n", mainServer);
    }

    ret = SP_connect((char *) mainServer,
            user,
            0, 1, &Mbox,
            Private_group);

    if (ret < 0) {
        if(dbg) {
            SP_error(ret);
            printf("Failed.\nConnecting to %s\n", altServer);
        }

        ret = SP_connect((char *) altServer,
                user,
                0, 1, &Mbox,
                Private_group);

        if (ret < 0) {
            printf("SP_connect Failed.\n");
            setSymbol("CONNECTED", "false",UNLOCK,LOCAL);
            disconnectCache();
        } else {
            setSymbol("CONNECTED", "true",UNLOCK,LOCAL);
            server = altServer;
        }
    } else {
        setSymbol("CONNECTED", "true",UNLOCK,LOCAL);
        server = mainServer;

        if(dbg) {
            fprintf( debugOut, "... done\n");
        }
    }

    if(server != (char *)NULL) {
        tmp=strtok(server,"@");
        tmp=strtok(NULL," ");
        setSymbol("CONNECTED_TO",tmp,UNLOCK,LOCAL);
        sprintf(scratch,"#%s#%s", user,tmp);
        setSymbol("ME",scratch,LOCK,LOCAL);
    }

    if(ret < 0) {
        return (0);
    }
    SP_join(Mbox,"global");

    if( strcmp(group,"global")) {
        SP_join(Mbox, group);
    }

    action=getSymbol("ONJOIN");
    if(action) {
        fprintf( myStdout, "%s\n",action);
        fflush(myStdout);
    }
    tmp=getSymbol("ON_CONNECT");
    if(tmp) {
        if(strlen(tmp) > 0) {
            fprintf( myStdout, "%s\n",tmp);
            fflush(myStdout);
        }
    }

    return (1);
}

void spreadLeave(char *group) {
    char sql[BUFFSIZE];
    char *me;
    char *dataFile;
    char *defaultGroup;
    char *zErrMsg;
    char *tmp;
    int rows,columns;
    char **results = NULL;
    int i;
    //    int Mbox;

    int rc;
    sqlite3 *db;

    me=(char *)getSymbol("ME");
    defaultGroup=getSymbol("GROUP");
    dataFile=getSymbol("DATABASE");

    rc = sqlite3_open(dataFile, &db);


    if(group == (char *)NULL) {
        //        sprintf(sql,"select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group <> '%s';",me,defaultGroup);
        sprintf(sql,"select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group <> 'global';",me);
    } else {
        sprintf(sql,"select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group='%s';",me,group);
    }

    //    printf("Leave all except nodebrain\n");
    //    printf("%s\n",sql);

    rc = sqlite3_get_table(db, sql, &results, &rows, &columns, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut,"spreadLeave:ERROR:%s;\n",sql);
        fflush(debugOut);
    } else {
        for(i=1;i<=rows;i++) {
            tmp=getSymbol(results[i]);

            // Mbox=atoi(tmp);

            SP_leave(Mbox,results[i]);
            sprintf(sql,"update nodes set state='LEAVE' where name = '%s' and sp_group='%s'", me,results[i]);

            // printf("%s\n",sql);

            rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
            if(rc != SQLITE_OK) {
                fprintf(debugOut,"spreadLeave:ERROR:%s;\n",sql);
                fflush(debugOut);
            }

            sprintf(sql,"update nodes set state='UNKNOWN' where name <> '%s' and sp_group='%s'", me,results[i]);
            rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
            if(rc != SQLITE_OK) {
                fprintf(debugOut,"spreadLeave:ERROR:%s;\n",sql);
                fflush(debugOut);
            }
        }
    }
    sqlite3_close(db);
}

int spreadJoin(char *group) {
    //	char           *server;
    char           *user;
    char           *connected;
    int             status = 0;
    char buffer[BUFFSIZE];

    //	server = (char *) getSymbol("server");
    user = (char *) getSymbol("USER");
    connected = (char *) getSymbol("CONNECTED");

    if (!strcmp(connected, "false")) {
        //Not connected, so leave
        fprintf(debugOut,"Not Connected\n");
        fflush(debugOut);
        status = 1;
        return status;
    }
    SP_join(Mbox, group);
    sprintf(buffer,"%d",Mbox);

    return status;


}

    void
mkNodeDeclare(char *ptr, char *buffer)
{
    char            scratch[BUFFSIZE];
    strcpy(scratch, ptr);

    char           *tmp, *p1, *p2, *p3;

    tmp = (char *) strtok(scratch, ";\n");
    p1 = (char *) strtok(tmp, "@");
    p2 = (char *) strtok(NULL, ":");
    p3 = (char *) strtok(NULL, " ");

    sprintf(buffer, "declare %s brain %s@%s:%s;", p2, p1, p2, p3);

}
/*
 * Should this command act on the data defined in the toSpread cache, and notify 
 * the client of a change ?
 *
 * If so then it would be better to set up a communication variable name space.
 *
 * e.g. A remote system can do ^set TST done, the local client would read this by doing:
 *
 * ^get COMMS.TST
 *
 * and set it by:
 *
 * ^set COMMS.TST pending
 *
 * A config parameters would be defined
 * 
 * COMMS_VARS     true|false  If true the comms variable namespace is active
 * REMOTE_CREATE  true|false  If true remote ^set commands will create the variable, 
 * if it does not exist.
 *
 * ON_CHANGE      <cmd>     If a set command changes a variable run this command.  
 *                          If null or undefined do nothing.
 *
 */
void rxSet(char *name,char *value) {
    char *client;
    char *fmt;
    char *cv;
    char *cr;

    char buffer[255];
    char scratch[255];
    char scr[255];


    char *tmp;
    char *tmp1;

    client = getSymbol("CLIENT");
    fmt = getSymbol("SET_FMT");
    cv= getSymbol("COMMS_VARS");
    cr=getSymbol("ADD_CR");

    //    printf("COMMS_VARS=%s\n",cv);

    if(!strcmp(client,"raw")) {
        fprintf( myStdout, fmt,name,value);
    } else if(!strcmp(client,"redis-cli")) {
        fprintf(myStdout,"set %s %s\n",name,value);
    } else if(!strcmp(client,"nodebrain")) {
        if((strchr(name,'.') != (char *)NULL) && ( *(name + (strlen(name)-1) ) == '.' )   ) {
            fprintf(myStdout,"%s assert %s\n",name,value);
        } else {
            fprintf(myStdout,"alert %s=%s;\n",name,value);
        }
    } else if(!strcmp(client,"tcl")) {
        fprintf(myStdout,"set %s %s\n",name,value);
    } else if(!strcmp(client,"python") || !strcmp(client,"ruby")) {
        fprintf(myStdout,"%s=%s\n",name,value);
    } else if(!strcmp(client,"forth")) {
        fprintf(stderr,"Client %s unknown\n",client);
        fprintf(myStdout, "s\" %s\" %s\n", value, name);

        //
        //         ficl assumed the use of ficl objects.
        //         
        //         NOTE: This needs some thought.
        //         
        //         The problem is that forth does not suffer fools.  If you do something stupid, bad things will happen.
        //         
        //         The alternatives are:
        //            Have a more tolerent command processor, which will simply error if it doen't understand.
        //         The problem is that forth does not suffer fools.  
        //         If you do something stupid, bad things will happen.
        //         
        //         The alternatives are:
        //            Have a more tolerent command processor, which will simply error 
        //            if it doen't understand.
        //            
        //            Treat the strings recieved as commands for parsing.

        //        strcpy(scr, name);
        //        fprintf(myStdout,"%s to %s\n", value, name);

        //        tmp = (char *)strtok(scr,".");
        //        tmp1 = (char *)strtok(NULL,".");
        //        
        //        if(tmp1) {
        //            sprintf(scratch,"%s --> .%s",tmp,tmp1);
        //        } else {
        //            strcpy(scratch,name);
        //        }
        //        
        //        if (value[0] == '"' ) {
        //            sprintf(buffer,"s\" %s %s swap move", &value[1],scratch);
        //        } else {
        //            sprintf(buffer, "%s to %s", value,scratch);
        //        }
        //        
        //        setSymbol("CONNECTED","false",UNLOCK,LOCAL);
        //        printf("%s\n",buffer);

    } else {
        fprintf(stderr,"Client %s unknown\n",client);
    }

    if(!strcmp(cr,"true")) {
        fprintf(myStdout,"\n");
    }
    fflush(myStdout);
}

void rxExit() {
    char *in;
    char *out;

    if(!strcmp(getSymbol("EXIT_COMMAND"),"true")) {
        fprintf(myStdout,"stop\n");
        in=getSymbol("STDIN");
        out=getSymbol("STDOUT");

        if( strcmp(in,"/dev/tty"))  {
            unlink( in );
        }
        if( strcmp(out,"/dev/tty"))  {
            unlink( out );
        }

        fprintf(debugOut,"exit from toSpread.\n");
        fflush(debugOut);

        exit(0);
    }
}

void cleanCache() {
    char *dbName;
    sqlite3 *mydb;
    int rc;
    time_t now;
    char sql[255];
    char *zErrMsg;
    char *autoPurge;
    int ttl;
    char *tmp;



    while (1) {
        sleep(10);
        tmp=getSymbol("STALE_AFTER");
        ttl=atoi(tmp);

        sprintf(sql,"delete from nodes where state <> 'CONNECTED' and stale < %d;",(int)now);
        autoPurge = getSymbol("AUTOPURGE");
        if(!strcmp(autoPurge,"true")) {
            dbName = getSymbol("DATABASE");        
            //        printf("Nap time.\n");


            now = time(NULL);
            sprintf(sql,"delete from nodes where state <> 'CONNECTED' and stale < %d;",(int)now);

            //            printf("%s\n",sql);

            pthread_mutex_lock(&cacheLock);

            rc = sqlite3_open(dbName, &mydb);

            rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);

            sqlite3_close(mydb);
            pthread_mutex_unlock(&cacheLock);
            sleep(50);
        }
    }
}

void spreadRX() {

    int             service_type = 0;
    int             ret;
    char            sender[MAX_GROUP_NAME];
    int             num_groups;
    char            target_groups[100][MAX_GROUP_NAME];
    int16           mess_type;
    static char     message[MAX_MESSLEN];
    int             endian_mismatch;

    char           *me;
    char *debug;
    int debugFlag=0;
    char            buffer[BUFFSIZE];
    char *cmd,*p1,*p2;
    char *ign;
    int flag=0;
    //    char *client;
    int connected=0;
    int i=0;
    //    char *onjoin;
    //    char *onjoindelay;


    membership_info memb_info;
    runFlag=1;

    while( 1 ) {
        do {
            connected=spreadConnect();

            if(connected==0) {
                if(debugFlag != 0) {
                    fprintf(stderr,"... waiting ...\n");
                }

                sleep(5); // try every 5 seconds, until success
            }
        } while (connected==0);

        me=getSymbol("ME");
        ign=getSymbol("IGN_GLOBAL");
        runFlag=1;

        while (runFlag) {
            do {
                ret = SP_receive(Mbox, &service_type, sender, 100,
                        &num_groups, target_groups,
                        &mess_type, &endian_mismatch, sizeof(message), message);

                if( ret < 0) {

                    switch(ret) {
                        case NET_ERROR_ON_SESSION:
                            runFlag =0 ;
                            break;
                        case CONNECTION_CLOSED:
                            fprintf(stderr,"connection closed\n");
                            break;
                        default:
                            SP_error(ret);
                    }
                }

                debug=getSymbol("DEBUG");
                debugFlag=strcmp(debug,"false");
            } while((!strcmp(sender,me)) && (!strcmp(getSymbol("OWN_MESSAGES"), "false"))) ;


            flag= (!strcmp(ign,"false")) || ((num_groups == 1) && (!strcmp(ign,"true")) && (!strcmp(target_groups[0],me)));

            if(debugFlag != 0) {
                fprintf(debugOut,"Flag = %d\n",flag);
                fprintf(debugOut,"R:%s\n",message);
                fprintf(debugOut,"Message sent to %d recipients\n",num_groups);

                for(i=0;i< num_groups;i++) {
                    fprintf(debugOut,"Message sent to %d:%s\n",i,target_groups[i]);
                    cacheAdd((char *) &target_groups[i], sender,CONNECTED);

                }
                fprintf(debugOut,"===========>\n");
            }

            if (Is_regular_mess(service_type)) {
                message[ret] = 0;

                // TODO add sender to cache, if not known.
            /*    if (!strcmp(sender, me)) {
                    if (!strcmp(getSymbol("OWN_MESSAGES"), "true")) {
                        fprintf(myStdout,"%s", message);
                    }
                    // } else  if (flag != 0) {
            } 
            */

            if (flag != 0) {
                if(!strcmp( getSymbol("SHOW_SENDER"),"true")) {
                    fprintf(myStdout,"%s ", sender);
                }

                strcpy(buffer, message);

                cmd=(char *)strtok(buffer," \n");
                // printf("\nmsg=%s\n",message);
                p1=(char *)strtok(NULL," \n");
                p2=(char *)strtok(NULL," \n");
                //
                // If message starts with ^ it is a command
                //
                if (message[0] == '^') {
                    if(!strcmp(cmd,"^set")) {
                        // printf("\np1=%s\np2=%s\n",p1,p2);
                        rxSet(p1,p2);
                    } else if(!strcmp(cmd,"^exit")) {
                        rxExit();
                    } else if(!strcmp(cmd,"^empty")) {
                        emptyCache(p1);
                    } else if(!strcmp(cmd,"^get")) {
                        if( debugFlag != 0) {
                            fprintf(debugOut,"%s wants to Get %s\n",sender,p1);
                        }
                        p2=getSymbol( p1 );

                        if(p2 == (char *)NULL) {
                            p2="NOT SET";
                        }


                        ret = SP_multicast(Mbox, AGREED_MESS, sender, 1, strlen(p2), p2);
                    }
                } else if(!strcmp( getSymbol("PASSTHROUGH"),"true")) {
                    // 
                    // If message does not start with ^ and pass through set then send
                    // what I have got.
                    //
                    if( !strcmp( getSymbol("ADD_CR"),"true")) {
                        p1=strtok(message,"\n");

                        fprintf(myStdout, "%s\n",p1);
                    } else {
                        fprintf(myStdout, "%s",message);
                    }
                }
                fflush(myStdout);
            }
            p1 = getSymbol("AUTOPURGE");
            if(!strcmp(p1,"true")) {
                cacheClean();
            }
            } else if (Is_membership_mess(service_type)) {
                ret = SP_get_memb_info(message, service_type, &memb_info);
                if (ret < 0) {
                    fprintf(debugOut,"BUG: membership message does not have valid body\n");
                    fflush(debugOut);
                    SP_error(ret);
                    exit(1);
                }
                if (Is_reg_memb_mess(service_type)) {
                    int             i;
                    char *client;

                    client=getSymbol("CLIENT");
                    if( debugFlag != 0) {
                        fprintf(debugOut,"%s:Received REGULAR membership for group %s with %d members, where I am member %d:\n",memb_info.changed_member,sender, num_groups, mess_type);
                        fflush(debugOut);
                    }

                    //                    cacheSetUnknown( sender );
                    if (Is_caused_join_mess(service_type)) {

                        if( debugFlag != 0) {
                            fprintf(debugOut,"%s:JOINED group %s\n",memb_info.changed_member,sender);
                            fflush(debugOut);
                        }

                        for (i = 0; i < num_groups; i++) {
                            if( debugFlag != 0) {
                                fprintf(debugOut,"\t%s\n",(char *) &target_groups[i][0]);
                                fflush(debugOut);
                            }
                            if( !lookupCacheGroup(&target_groups[i][0], sender)) {
                                cacheAdd((char *) &target_groups[i][0], sender,CONNECTED);
                            }
                            // ATH
                            cacheSetConnected((char *) &target_groups[i][0], sender,CONNECTED);
                        }

                    } else if (Is_caused_leave_mess(service_type)) {
                        if( debugFlag != 0) {
                            fprintf(debugOut,"%s:LEFT group %s\n",memb_info.changed_member,sender);
                            fflush(debugOut);
                        }
                        cacheSetConnected((char *) memb_info.changed_member, sender,LEAVE);
                    } else if (Is_caused_disconnect_mess(service_type)) {
                        if( debugFlag != 0) {
                            fprintf(debugOut,"spreadRX:%s:DISCONNECT group %s\n",memb_info.changed_member,sender);
                            fflush(debugOut);
                        }
                        cacheSetConnected((char *) memb_info.changed_member,sender, DISCONNECTED );
                    } else if (Is_caused_network_mess(service_type)) {
                        if( debugFlag != 0) {
                            int             num_vs_sets, j;
                            unsigned int my_vsset_index;
                            vs_set_info vssets[MAX_VSSETS];
                            char sql[1024];
                            char tmp[256];

                            char members[MAX_MEMBERS][MAX_GROUP_NAME];

                            //                            fprintf(debugOut,"spreadRX:%s:NETWORK group %s\n",memb_info.changed_member,sender);

                            num_vs_sets = SP_get_vs_sets_info(message, &vssets[0], MAX_VSSETS, &my_vsset_index);

                            //                            fprintf(debugOut,"... num_vs_sets=%d\n",num_vs_sets);
                            fflush(debugOut);


                            for (i = 0; i < num_vs_sets; i++) {

                                //                                printf("%s VS set %d has %u members:\n",(i == my_vsset_index) ?  ("LOCAL") : ("OTHER"), i, vssets[i].num_members);
                                ret = SP_get_vs_set_members(message, &vssets[i], members, MAX_MEMBERS);

                                for (j = 0; j < vssets[i].num_members; j++) {
                                    //                                    printf("\t%s\n", members[j]);

                                    sprintf(sql,"update nodes set flag=1 where name ='%s' ;",members[j]);
                                    execSQL( sql );
                                    //                                    printf("%s\n",sql);
                                }
                                sprintf(sql,"update nodes set state='UNKNOWN' where flag = 0 and sp_group ='%s';", sender);
                                //                                printf("%s\n",sql);
                                execSQL( sql );
                                sprintf(sql,"update nodes set flag=0;");
                                //                                printf("%s\n",sql);
                                execSQL( sql );

                            }

                        }
                    }


                    fflush(myStdout);
                }
            }
        }
        setSymbol("CONNECTED","false",UNLOCK,LOCAL);
    }
}

void setupDatabase() {
    char           *tmp;
    int dbExists;
    char safeBuffer[BUFFSIZE];
    char buffer[BUFFSIZE];
    int i;
    int rc;
    sqlite3 *db;
    char *zErrMsg;

    tmp=getSymbol("DATABASE");

    if(!tmp) {
        strcpy(safeBuffer,"/tmp/nb2spread.db");
        tmp=&safeBuffer[0];
        setSymbol("DATABASE",tmp,UNLOCK,GLOBAL);
        lockSymbol("DATABASE");
    }

    if( !strcmp(tmp,":memory:")) {
        dbExists=0;
    } else {
        i=open(tmp,O_RDONLY);
        if(i>0) {
            fprintf(debugOut,"Database %s exists\n",tmp);
            fflush(debugOut);
            dbExists=1;
            close(i);
        } else {
            dbExists=0;
        }
    }

    rc = sqlite3_open(tmp, &db);

    if(dbExists) {
        fprintf(debugOut,"DB existed at start, assume tables defined.\n");
        fflush(debugOut);

        strcpy(buffer,"update nodes set state='DISCONNECTED';");
        rc = sqlite3_exec(db, buffer, callback, 0, &zErrMsg);
    } else {
        fprintf(debugOut,"New DB, define tables\n");
        fflush(debugOut);

        strcpy(buffer,"create table nodes ( sp_group varchar(32), name varchar(32), state varchar(8) default 'UNKNOWN', touched integer, stale integer, flag integer default 0);");
        rc = sqlite3_exec(db, buffer, callback, 0, &zErrMsg);
        if(rc !=SQLITE_OK)
        {
            printf("Error on CREATE: %s.\n", zErrMsg);
            printf("%s\n",buffer);
        }

        strcpy(buffer,"create table symbol ( key varchar(32) unique, value varchar(32));");
        rc = sqlite3_exec(db, buffer, callback, 0, &zErrMsg);
        if(rc !=SQLITE_OK)
        {
            printf("Error on symbols CREATE: %s.\n", zErrMsg);
            printf("%s\n",buffer);
        }

    }
    sqlite3_close(db);

}

void startSpreadRX() {
    char            safeBuffer[BUFFSIZE];
    int             threadId;
    pthread_t       spread;

    lockSymbol("USER");
    //    sprintf(safeBuffer, "#%s#%s", getSymbol("USER"), getSymbol("HOSTNAME"));
    //    setSymbol("ME", safeBuffer,UNLOCK,LOCAL);
    //    lockSymbol("ME");

    //    spreadConnect();

    threadId = pthread_create(&spread, NULL, (void *) spreadRX, (void *) NULL);
    setSymbol("CONNECTED","true",UNLOCK,LOCAL);

}

int main(int argc, const char *argv[]) {
    char            buffer[BUFFSIZE];
    char           *status;
    char           *cmd;
    char           *p1;
    char           *p2;
    char *p3;

    char           *tmp;
    int             runFlag = 1;
    int             fromFile = 0;
    int             v;
    struct passwd *userRecord;

    FILE           *fp = stdin;

    char            safeBuffer[BUFFSIZE];
    int             ret;
    //    int i;
    int ch;
    int uid;
    char *user;
    char fifoScratch[255];
    //    char *fifo;
    int threadId;
    pthread_t       clean;
    char *stdinFifo=(char *)NULL;
    char *stdoutFifo=(char *)NULL;;

    int dbg;
    char *debug;

    debugOut = stderr;

    myStdin = stdin;
    myStdout = stdout;

    fifoScratch[0]=0x00;
    int multiLine=0;
    char *mPtr=(char *)NULL;

    //    (void)daemon(1,1);
    strcpy(buffer, "false");
    while((ch = getopt(argc, ( char **)argv,"hc:u:d:fg:l:n:p:i:o:")) != -1) {
        switch(ch) {
            case 'h':
                usage();
                exit(0);
                break;
            case 'c':
                setSymbol("START_FILE", (char *) optarg, LOCK,LOCAL);
                break;
            case 'd':
                setSymbol("DATABASE",(char *)optarg,LOCK,GLOBAL);
                break;
            case 'u':
                setSymbol("USER", (char *) optarg, LOCK,LOCAL);
                break;
            case 'n':
                setSymbol("HOSTNAME", (char *) optarg, LOCK,LOCAL);
                break;
            case 'f':
                tmp=getSymbol("START_FILE");
                unlink(tmp);
                break;
            case 'g':
                setSymbol("GROUP",(char *)optarg,LOCK,GLOBAL);
                break;
            case 'l':
                debugOut = fopen(optarg,"a");
                if(!debugOut) {
                    debugOut = stderr;
                } else {
                    setSymbol("DEBUG","true",LOCK,GLOBAL);
                    setSymbol("LOG_FILE",optarg,LOCK,LOCAL);
                }
                break;
            case 'p':
                sprintf(fifoScratch,"%s.%%d",optarg);
                break;
            case 'i': // fifo for stdin
                stdinFifo=(char *)strsave(optarg);
                break;
            case 'o':
                stdoutFifo=(char *)strsave(optarg);
                break;
                break;
        }
    }
    /*
     * Set defaults.
     *
     */
    setSymbol("DEBUG", "false", UNLOCK,GLOBAL);
    setSymbol("SHOW_SENDER", "false", UNLOCK,GLOBAL);     // prefix a message with it's sender
    setSymbol("EXIT_COMMAND", "false", UNLOCK,GLOBAL);  // If true enable ^exit
    setSymbol("OWN_MESSAGES", "false", UNLOCK,GLOBAL);  // If true xfer messages from self
    setSymbol("SAVE_ALLOWED", "true", UNLOCK,GLOBAL);   // Allow change to be written to start up file.
    setSymbol("CONNECTED", "false", UNLOCK,LOCAL);      // True if connected

    setSymbol("CONFIGURED","false",UNLOCK,LOCAL);      // True if setup & and connected
    setSymbol("CLIENT","raw",UNLOCK,GLOBAL);            // What sort of client am I
    setSymbol("AUTOCONNECT","false",UNLOCK,GLOBAL);     // Connect on start
    setSymbol("PASSTHROUGH","true",UNLOCK,GLOBAL);     // Anything not prefixed witha ^ is passed through

    setSymbol("INTERACTIVE","false",UNLOCK,GLOBAL);     // Set true if a human is reading my messages.
    setSymbol("BUILD",__DATE__,LOCK,LOCAL);             // Build date
    setSymbol("CLEARDB","false",UNLOCK,GLOBAL);         // delete db at start
    setSymbol("SET_FMT","%s %s",UNLOCK,GLOBAL);         // Output format when CLIENT is raw
    setSymbol("AUTOPURGE","false",UNLOCK,GLOBAL);       // Purge cache nodes that are not connected and stale
    setSymbol("STALE_AFTER","600",UNLOCK,GLOBAL);      // If AUTOPURGE age, in seconds, of items to purge
    setSymbol("ADD_CR","false",UNLOCK,GLOBAL);
    setSymbol("IGN_GLOBAL","false",UNLOCK,GLOBAL);       // if true ignore messages sent to anything but my private group.

    // See comments at rxSet
    setSymbol("COMMS_VARS","false",UNLOCK,GLOBAL);    // If true the comms variable namespace is active
    setSymbol("REMOTE_CREATE","false",LOCK,GLOBAL); // If true remote ^set commands will create the variable, if it does not exist.
    setSymbol("ON_CHANGE","",LOCK,GLOBAL);          // If a set command changes a variable run this command.  If null or undefined do nothing.
    setSymbol("ON_CONNECT","",UNLOCK,GLOBAL);       // Issue this string on a succesfull connect to spread.

    //    setSymbol("RUN_SLAVE","false",UNLOCK,GLOBAL);       // if set to true, will run designated slave and attach to stdin & stdout
    //    setSymbol("SLAVE","/usr/bin/false",UNLOCK,GLOBAL);


    if(!getSymbol("USER")) {
        uid = getuid();
        userRecord = getpwuid(uid);
        setSymbol("USER",userRecord->pw_name,UNLOCK,LOCAL);
    }

    if(!getSymbol("GROUP")) {
        setSymbol("GROUP", "global", UNLOCK,GLOBAL);
    }

    if(!getSymbol("DATABASE")) {
        user = getSymbol("USER");
        sprintf(buffer,"/var/tmp/%s.db",user);
        setSymbol("DATABASE",buffer,UNLOCK,GLOBAL);
    }
    setSymbol("SERVER","4803",UNLOCK,GLOBAL);
    setSymbol("ALTSERVER","4803",UNLOCK,GLOBAL);

    if(!getSymbol("START_FILE")) {
        tmp=getenv("HOME");
        sprintf(safeBuffer,"%s/.start.rc", tmp);
        setSymbol("START_FILE",safeBuffer,UNLOCK,LOCAL);
    }

    if (! getSymbol("HOSTNAME")) {
        v = gethostname(buffer, BUFFSIZE);
        tmp = (char *) strtok(buffer, ".");

        if (tmp) {
            setSymbol("HOSTNAME", tmp, LOCK,LOCAL);
        }
    }

    if ( getSymbol("LOG_FILE")) {
        myStderr=fopen( getSymbol("LOG_FILE"),"w");
    } else {
        myStderr = stderr;
    }

    fp = fopen(getSymbol("START_FILE"), "r");

    if (fp) {
        fromFile = 1;
    } else {
        /*
         *           saveSymbols();
         *           fp = fopen(getSymbol("START_FILE"), "r");
         *           fromFile = 1;
         */
        fprintf(stderr,"A startup file MUST exist, even if it is empty\n");
        fprintf(stderr,"The default name is $HOME/.start.rc\n");
        fprintf(stderr,"Safe buffer is %s\n", safeBuffer);
        exit(1);
    }
    /*
     *       ^set <param> <value>
     *       ^get <param>
     * 
     *       ^register <node>        Send declare command to spread
     *       ^update                 Send all know dirty nodes, and mark as clean.
     *       ^flush                  Send all known nodes.
     *       ^add <node> <grp>       Add to local cache only.
     * 
     *       ^del <node>
     *       ^del <node> <grp>
     * 
     *       ^save | ^save state
     *       ^save cache
     *       ^save settings
     * 
     *       ^load | ^load state
     *       ^load cache
     *       ^load settings
     * 
     *       ^join                   Join default (first defined) group
     *       ^join <group>
     * 
     *       ^leave                  Leave all groups
     *       ^leave <group>
     * 
     *       ^disconnect             Terminate spread connection
     * 
     *       ^empty                  Delete cache entries that are disconnected.
     * 
     *       ^send <group> <nb command>     Send nb command as given. NOT IMPLEMENTED.
     * 
     *       ^connected <name>      return TRUE if name is connected, FALSE otherwise.
     */    

    debug = (char *)getSymbol("DEBUG");
    dbg = ( !strcmp(debug,"true") );

    while (runFlag) {
        while ((status = fgets(buffer, BUFFSIZE, fp)) != 0) {
            strcpy(safeBuffer, buffer);

            if( (0 != dbg ) && (0 == fromFile))  {
                fprintf( myStderr,"C:stdin:%s\n",safeBuffer);
                fflush( myStderr );
            }

            tmp = (char *) strtok(buffer, "\n");

            if (tmp) {
                if (strlen(tmp) > 0) {
                    cmd = (char *) strtok(tmp, " ");
                    if (cmd[0] == '^') {
                        if (!strcmp(cmd, "^set")) {
                            p1 = (char *) strtok(NULL, " ");
                            p2 = (char *) strtok(NULL, "\n");

                            if ((p1 != (char *) NULL) && (p2 != (char *) NULL)) {
                                setSymbol(p1, p2, 1,GLOBAL);
                            }
                        } else if (!strcmp(cmd, "^set+lock")) {
                            p1 = (char *) strtok(NULL, " ");
                            p2 = (char *) strtok(NULL, "\n");

                            if ((p1 != (char *) NULL) && (p2 != (char *) NULL)) {
                                setSymbol(p1, p2, 1,GLOBAL);
                                lockSymbol(p1);
                            }

                        } else if (!strcmp(cmd, "^get")) {
                            p1 = (char *) strtok(NULL, " ");
                            if(p1 != (char *)NULL) {
                                p2=getSymbol(p1);
                                p3=getSymbol("CLIENT");

                                if(!strcmp(p3,"nodebrain")) {
                                    fprintf(myStdout,"alert %s=\"%s\";\n",p1,p2);
                                } else if(!strcmp(p3, "forth")) {
                                    if(!strcmp(p2,"true")) {
                                        fprintf(myStdout,"-1\n");
                                    } else if(!strcmp(p2,"false"))  {
                                        fprintf(myStdout,"0\n");
                                    } else
                                        fprintf(myStdout,"s\" %s\"\n",p2);
                                } else {
                                    fprintf(myStdout,"%s\n",p2);
                                }
                                fflush(myStdout);
                            }

                        } else if (!strcmp(cmd, "^connected")) {
                            //	      p1 = (char *) strtok(NULL, " ");
                            p1 = (char *) getSymbol("CONNECTED");
                            p3=getSymbol("CLIENT");

                            if(!strcmp(p3,"nodebrain")) {
                                if(!strcmp( p1,"true" ) ) {
                                    printf("alert CONNECTED=\"true\"\n");
                                } else {
                                    fprintf(myStdout,"alert CONNECTED=\"false\"\n");
                                }
                            } else if (!strcmp(p3,"raw")) {
                                if(!strcmp( p1,"true" ) ) {
                                    fprintf(myStdout,"TRUE\n");
                                } else {
                                    fprintf(myStdout,"FALSE\n");
                                }

                            }

                            fflush(myStdout);

                        } else if (!strcmp(cmd, "^add")) {
                            p1 = (char *) strtok(NULL, " ");
                            p2 = (char *) strtok(NULL," \n");

                            if (p1 && p2) {
                                cacheAdd(p1, p2, DISCONNECTED);
                            }
                        } else if (!strcmp(cmd, "^del")) {
                            p1 = (char *) strtok(NULL, " ");
                            p2 = (char *) strtok(NULL," \n");

                            cacheRemove(p1,p2);

                        } else if (!strcmp(cmd, "^lock")) {
                            p1 = (char *) strtok(NULL, " ");
                            if (p1 != (char *) NULL) {
                                lockSymbol(p1);
                            }
                        } else if (!strcmp(cmd, "^who")) {
                            cacheDump();
                        } else if (!strcmp(cmd, "^dump")) {
                            p1 = (char *) strtok(NULL," \n");

                            if(!p1) {
                                dumpSymbols();
                                cacheDump();
                            } else {
                                if(!strcmp(p1,"cache")) {
                                    cacheDump();
                                }
                                if(!strcmp(p1,"symbols")) {
                                    dumpSymbols();
                                }
                            }
                        } else if (!strcmp(cmd, "^connect")) {
                            char *c;

                            c=getSymbol("CONNECTED");

                            if(!strcmp(c,"false")) {
                                startSpreadRX();
                            }
                        } else if (!strcmp(cmd, "^exit")) {
                            p1 = getSymbol("EXIT_COMMAND");

                            if (!strcmp(p1, "true")) {
                                runFlag = 0;
                                break;
                            }
                            //;
                        } else if (!strcmp(cmd, "^save")) {
                            p1 = (char *) strtok(NULL, " ");
                            if (p1 == (char *) NULL) {
                                setSymbol("CONFIGURED","true",UNLOCK,GLOBAL);
                                saveSymbols();
                            } else {
                                if (!strcmp(p1, "cache")) {
                                } else if (!strcmp(p1, "settings")) {
                                    setSymbol("CONFIGURED","true",UNLOCK,GLOBAL);
                                    saveSymbols();
                                } else if (!strcmp(p1, "state")) {
                                    saveSymbols();
                                }
                            }
                        } else if (!strcmp(cmd, "^empty")) {
                            p1 = (char *) strtok(NULL, " ");
                            emptyCache(p1);
                        } else if (!strcmp(cmd, "^join")) {
                            int             status = 0;

                            p1 = (char *) strtok(NULL, " ");
                            if (p1 != (char *) NULL) {
                                status = spreadJoin(p1);
                            }
                        } else if (!strcmp(cmd, "^leave")) {
                            p1 = (char *) strtok(NULL, " ");
                            spreadLeave(p1);

                        } else if(!strcmp(cmd,"^help")) {
                            printf("Build date %s\n",__DATE__);

                            printf("^dump <cache|symbols>\n");
                            printf("^set <param> <value>\n");
                            printf("^set+lock <param> <value>\n");
                            printf("^get <param>\n");
                            //                            printf("^register <node>        Send declare command to spread\n");
                            //                            printf("^update                 Send all know dirty nodes, and mark as clean.\n");
                            //                            printf("^flush                  Send all known nodes.\n");
                            printf("^add <node>             Add to local cache only.\n");
                            printf("^save | ^save state\n");
                            printf("^save cache\n");
                            printf("^save settings\n");
                            //                            printf("^load | ^load state\n");
                            //                            printf("^load cache\n");
                            //                            printf("^load settings\n");
                            //                            printf("^join                   Join default (first defined) group\n");
                            printf("^join <group>\n");
                            printf("^leave                  Leave all groups\n");
                            printf("^leave <group>\n");
                            printf("^disconnect             Terminate spread connection\n");
                            printf("^empty                  Delete cache entries that are in state disconnected.\n");
                            printf("^empty all              Delete cache entries that are not in state connected.\n");
                            printf("^send <group> <textd>\tSend text to group as given.\n");

                            printf("^start\t\t\tStart a block of lines.\n");
                            printf("^end\t\t\tEnd a block of lines.\n");
                            //                        } else if (!strcmp(cmd, "^flush")) {
                            //							cacheTouchAll();
                    } else if (!strcmp(cmd, "^disconnect")) {
                        spreadDisconnect();
                    } else if (!strcmp(cmd, "^send")) {
                        p1 = (char *)strtok(NULL," ");
                        p2 = (char *)strtok(NULL,"\n");
                        strcat(p2,"\n");
                        ret = SP_multicast(Mbox, AGREED_MESS, p1, 1, strlen(p2), p2);
                    } else if(!strcmp(cmd,"^start")) {
                        multiLine = 1;
                        if( mPtr ) {
                            free(mPtr);
                            mPtr = (char *)NULL;
                        }
                    } else if(!strcmp(cmd,"^end")) {    
                        multiLine = 0;
                        //                            printf("%s",mPtr);
                        ret = SP_multicast(Mbox, AGREED_MESS, getSymbol("GROUP"), 1, strlen(mPtr), mPtr);
                        if( mPtr ) {
                            free(mPtr);
                            mPtr = (char *)NULL;
                        }
                    }
                    } else if(cmd[0] == '#') {
                        // comment
                    }  else {
                        //message to go.
                        if(multiLine) {
                            //                            printf("\tMULTI\n");
                            if(!mPtr) {
                                mPtr = strsave(safeBuffer);
                            } else {
                                char *t1;
                                t1 = malloc(strlen(mPtr) + strlen(safeBuffer) + 1 );
                                strcpy(t1,mPtr);
                                free(mPtr);
                                mPtr=t1;
                                strcat(mPtr,safeBuffer);
                            }
                        } else {
                            ret = SP_multicast(Mbox, AGREED_MESS, getSymbol("GROUP"), 1, strlen(safeBuffer), safeBuffer);
                        }
                    }
                }
            }
        }
        if (fromFile != 0) {
            char *autoConnect;
            //            char *runSlave;
            //            char *slave;
            char *clearDB;

            // check & set DATABASE HERE
            fromFile = 0;
            fclose(fp);

            fp = stdin;

            clearDB=getSymbol("CLEARDB");
            if(!strcmp( clearDB,"true")) {
                unlink( getSymbol("DATABASE"));
            }
            setupDatabase();

            /*
             *               runSlave=getSymbol("RUN_SLAVE");
             *               if(!strcmp( runSlave,"true")) {
             *               fprintf(debugOut, "Start slave ...\n");
             *               slave=getSymbol("SLAVE");
             *               fprintf(debugOut,"... %s\n",slave);
             } else {
             fprintf(debugOut, "NO slave\n");
             }
             */

            autoConnect=getSymbol("AUTOCONNECT");

            if(autoConnect != (char *)NULL) {
                if(!strcmp(autoConnect,"true")) {
                    startSpreadRX();
                }
            }

        } else {
            runFlag = 0;
            sprintf(safeBuffer, "disconnect %s\n", getSymbol("ME"));
            ret = SP_multicast(Mbox, AGREED_MESS, getSymbol("GROUP"), 1, strlen(safeBuffer), safeBuffer);
            exit(0);
        }

        if(strlen(fifoScratch) > 0) {
            struct stat dir_stat;

            sprintf(buffer,fifoScratch,0);  // stdin

            setSymbol("STDIN",buffer,LOCK,LOCAL);

            mkfifo(buffer,0600);
            myStdin = fopen(buffer,"r");

            sprintf(buffer,fifoScratch,1); // stdout
            setSymbol("STDOUT",buffer,LOCK,LOCAL);
            mkfifo(buffer,0600);
            myStdout = fopen(buffer,"w");
            fp = myStdin;
        } else {
            if(stdinFifo != (char *)NULL) {
                mkfifo(stdinFifo,0600);
                myStdin = fopen(stdinFifo,"r");
                fp = myStdin;
            }
            if(stdoutFifo != (char *)NULL) {
                mkfifo(stdoutFifo,0600);
                myStdout = fopen(stdoutFifo,"w");
            }
            setSymbol("STDIN","/dev/tty",LOCK,LOCAL);
            setSymbol("STDOUT","/dev/tty",LOCK,LOCAL);
        }
        p1=getSymbol("START_MSG");
        if(p1 != (char *)NULL) {
            fprintf( myStdout, "%s\n",p1);
            fflush(myStdout);
        }
        threadId = pthread_create(&clean, NULL, (void *) cleanCache, (void *) NULL);
    }
    return 0;
}
