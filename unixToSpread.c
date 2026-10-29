/*
 * Date: Thu 26 Feb 2009 (Modified for Unix Domain Sockets)
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
#include <sys/socket.h>
#include <sys/un.h>

#include <pwd.h>

#include "sp.h"
#include "mine.h"
#define MAX_MESSLEN     102400
mailbox         Mbox;

#define BUFFSIZE 255
char           *getSymbol(char *name);
void            setSymbol(char *name, void *value, int string, int local);
#define CLEAN 0
#define DIRTY 1
#define UNKNOWN 0
#define DISCONNECTED 1
#define CONNECTED 2
#define LEAVE 3

#define LOCK 0
#define UNLOCK 1

#define SP_HASHSIZE 31
#define LOCAL 0
#define GLOBAL 1
#define MAX_VSSETS      10
#define MAX_MEMBERS 100

FILE *debugOut = NULL;
FILE *myStdin = NULL;
FILE *myStdout = NULL;
FILE *myStderr = NULL;

extern char *optarg;

int             runFlag = 1;
pthread_mutex_t hashLock;
pthread_mutex_t cacheLock;
struct globalDefinitions global;

struct nlist {
    char           *name;
    void           *def;
    int             ro;
    int             local;
    struct nlist   *next;
};

char *strsave(const char *s) {
    char           *p;
    if ((p = malloc(strlen(s) + 1)) != NULL)
        strcpy(p, s);
    return (p);
}

void usage() {
    printf("usage: unixToSpread -h -c <cfg> -u <user> -d <database> -f -n <hostname> -s <socket> -P\n\n");
    printf("\t-c <cfg>\tConfig file.\n");
    printf("\t-d <database>\tCache database.\n");
    printf("\t-f\t\tForce, remove default config.\n");
    printf("\t-g <group>\tDefault group to join.\n");
    printf("\t-h\t\tHelp.\n");
    printf("\t-l <logfile>\tLog file.\n");
    printf("\t-n <hostname>\tSet hostname.\n");
    printf("\t-s <socket>\tUnix domain socket path.\n");
    printf("\t-u <user>\tSpread user.\n");
    printf("\t-P\t\tPrint config and exit.\n\n");
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    NotUsed = 0;
    int i;
    printf("CALLBACK\n");
    for(i = 0; i < argc; i++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i]: "NULL");
    }
    printf("\n");
    return 0;
}

int lookupCache(char *name) {
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows, columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql, "select count(*) from nodes where name='%s';", name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut, "lookupCache fatal DB error.\n");
        fflush(debugOut);
        exit(-1);
    }
    cnt = atoi(results[columns]);
    sqlite3_close(mydb);
    return(cnt);
}

int lookupCacheGroup(char *name, char *group) {
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows, columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql, "select count(*) from nodes where sp_group='%s' and name='%s';", group, name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut, "lookupCacheGroup fatal DB error.\n");
        fflush(debugOut);
        exit(-1);
    }
    cnt = atoi(results[columns]);
    sqlite3_close(mydb);
    return(cnt);
}

void cacheTouch(char *name) {
    sqlite3 *mydb;
    char *dbName;
    char sql[BUFFSIZE];
    char *zErrMsg;
    int rc;
    time_t now = time(NULL);

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql, "update nodes set touched=%d where name='%s';", (int)now, name);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut, "cacheTouch:UPDATE:%s\n", zErrMsg);
        fflush(debugOut);
    }
    sqlite3_close(mydb);
}

void cacheSetConnected(char *name, char *group, int c) {
    char *dbName;
    int rc = 0;
    char sql[BUFFSIZE];
    char tmp[16];
    char *zErrMsg;
    char *t1;

    sqlite3 *mydb;
    time_t now, stale;
    now = time(NULL);

    dbName = getSymbol("DATABASE");
    t1 = (char *)getSymbol("STALE_AFTER");

    stale = now + atoi(t1);
    rc = sqlite3_open(dbName, &mydb);

    switch(c) {
        case CONNECTED: strcpy(tmp, "CONNECTED"); break;
        case DISCONNECTED: strcpy(tmp, "DISCONNECTED"); break;
        case LEAVE: strcpy(tmp, "LEAVE"); break;
        default: strcpy(tmp, "UNKNOWN"); break;
    }

    sprintf(sql, "update nodes set state='%s',touched=%d,stale=%d where name='%s' and sp_group='%s';", tmp, (int)now, (int)stale, name, group);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut, "cacheSetConnected:update:%s\n", zErrMsg);
        fflush(debugOut);
    }
    sqlite3_close(mydb);
}

int isConnected(char *name) {
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows, columns;
    char *zErrMsg;
    int cnt;

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql, "select count(*) from nodes where name='%s' and state = 'CONNECTED';", name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK ) {
        fprintf(debugOut, "isConnected fatal DB error.\n");
        fflush(debugOut);
        exit(-1);
    }
    cnt = atoi(results[columns]);
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
    int rows, columns;
    int i, j;
    time_t clock;
    char *tmp;
    int interactive = 0;

    tmp = getSymbol("INTERACTIVE");
    if (tmp) {
        interactive = (strcmp(tmp, "false") != 0);
    }

    dbName = getSymbol("DATABASE");
    rc = sqlite3_open(dbName, &mydb);
    strcpy(output, "|");

    sql = "select name, sp_group, state, touched from nodes order by name, sp_group, touched;";
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if(rc != SQLITE_OK) {
        fprintf(debugOut, "DBERROR:cacheDump:%s\n", zErrMsg);
        fflush(debugOut);
    } else {
        if(interactive) {
            fprintf(myStdout, "================================================================================\n");
            fprintf(myStdout, "|Name                     |Group          |State       |Touched\n");
            fprintf(myStdout, "================================================================================\n");
        } else {
            fprintf(myStdout, "START_CACHE\n");
        }
        for(i = 1; i <= rows; i++) {
            for(j = 0; j < columns; j++) {
                switch(j) {
                    case 0:
                        if(interactive) fprintf(myStdout, "|%-25s|", results[(i*columns)+j]);
                        else fprintf(myStdout, "%s:", results[(i*columns)+j]);
                        break;
                    case 1:
                        if(interactive) fprintf(myStdout, "%-15s|", results[(i*columns)+j]);
                        else fprintf(myStdout, "%-s:", results[(i*columns)+j]);
                        break;
                    case 2:
                        if(interactive) fprintf(myStdout, "%-12s|", results[(i*columns)+j]);   
                        else fprintf(myStdout, "%s:", results[(i*columns)+j]);   
                        break;
                    case 3:
                        clock = (time_t) atoi(results[(i*columns)+j]);
                        if(interactive) fprintf(myStdout, "%s", asctime(localtime(&clock)));
                        else fprintf(myStdout, "%d:\n", (int)clock);
                        break;
                    default:
                        fprintf(myStdout, "%-5s|", results[(i*columns)+j]);
                        break;
                }
            }
        }
        if(interactive) {
            fprintf(myStdout, "================================================================================\n");
        } else {
            fprintf(myStdout, "END_CACHE\n");
        }
    }
    fflush(myStdout);
}

void disconnectCache() {
    char sql[BUFFSIZE];
    sqlite3 *db;
    char *dataFile;
    char *zErrMsg;
    int rc;

    sprintf(sql, "update nodes set state = 'DISCONNECTED';");
    dataFile = getSymbol("DATABASE");

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dataFile, &db);
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut, "disconnectCache:UPDATE:%s\n", zErrMsg);
        fflush(debugOut);
    }
    sqlite3_close(db);
    pthread_mutex_unlock(&cacheLock);
}

void emptyCache(char *all) {
    sqlite3 *mydb;
    char *dbName;
    char *me;
    int rc;
    char *zErrMsg;
    char sql[BUFFSIZE];
    time_t now = time(NULL);

    me = getSymbol("ME");
    dbName = getSymbol("DATABASE");

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);

    if (all == (char *)NULL) {
        sprintf(sql, "delete from nodes where state = 'DISCONNECTED' and name <> '%s';", me);
    } else if(!strcmp(all, "all")) {
        sprintf(sql, "delete from nodes where state <> 'CONNECTED';");
    } else if(!strcmp(all, "stale")) {
        sprintf(sql, "delete from nodes where state <> 'CONNECTED' and stale < %d;", (int)now);
    }

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut, "emptyCache:DELETE:%s\n", zErrMsg);
        fflush(debugOut);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

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

    sprintf(sql, "delete from nodes where name='%s'", n);
    if(g) {
        sprintf(scratch, " and sp_group='%s';", g);
    } else {
        strcpy(scratch, ";");
    }
    strcat(sql, scratch);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut, "DBERROR:cacheDelete:%s\n", zErrMsg);
        fflush(debugOut);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

void cacheAdd(char *n, char *g, int connect) {
    int rc = 0;
    int cnt = 0;
    sqlite3 *mydb;
    char *dbName;
    char sql[BUFFSIZE];
    char status[32];
    char **results = NULL;
    int rows, columns;
    char *zErrMsg;
    time_t now = time(NULL);

    dbName = getSymbol("DATABASE");
    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);

    sprintf(sql, "select count(*) from nodes where name='%s' and sp_group='%s';", n, g);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);
    if(rc != SQLITE_OK ) {
        fprintf(debugOut, "DBERROR:%s\n", zErrMsg);
        fflush(debugOut);
    }
    cnt = atoi(results[columns]);

    switch(connect) {
        case CONNECTED: strcpy(status, "CONNECTED"); break;
        case DISCONNECTED: strcpy(status, "DISCONNECTED"); break;
        case LEAVE: strcpy(status, "LEAVE"); break;
        default: strcpy(status, "UNKNOWN"); break;
    }

    if (cnt == 0) {
        sprintf(sql, "insert into nodes ( sp_group, name, state, touched ) values('%s','%s','%s',%d);", g, n, status, (int)now);
    } else {
        sprintf(sql, "update nodes set state='%s',touched=%d where name='%s' and sp_group='%s';", status, (int)now, n, g);
    }

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if(rc != SQLITE_OK) {
        fprintf(debugOut, "cacheAdd:INSERT:%s\n", zErrMsg);
        fflush(debugOut);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

int execSQL(char *sql) {
    char *zErrMsg;
    int rc;
    sqlite3 *mydb;
    char *dbName;

    dbName = getSymbol("DATABASE");
    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
    return(rc);
}

static struct nlist *hashtab[SP_HASHSIZE];

void dumpSymbols() {
    struct nlist   *np;
    int             i;
    char            fmt[BUFFSIZE];
    char *tmp;
    int interactive = 0;

    tmp = getSymbol("INTERACTIVE");
    if (tmp) {
        interactive = (strcmp(tmp, "false") != 0);
    }

    if(interactive) {
        fprintf(myStdout, "==============================================================|\n");
        fprintf(myStdout, "|\tBuild Date:%s\n", getSymbol("BUILD"));
        fprintf(myStdout, "==============================================================|\n");
        strcpy(fmt, "|%-20s|%-25s|%-6s|%-7s|\n");
        fprintf(myStdout, fmt, "Name", "Value", "Locked", "Scope");
        fprintf(myStdout, "==============================================================|\n");
    } else {
        fprintf(myStdout, "START_SYMBOLS\n");
    }
    for (i = 0; i < SP_HASHSIZE; i++) {
        if (hashtab[i] != 0) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if(interactive) {
                    fprintf(myStdout, "|%-20s", np->name);
                    fprintf(myStdout, "|%-25s", (char *) np->def);
                    if (np->ro == LOCK) fprintf(myStdout, "|%-6s|", "Yes");
                    else fprintf(myStdout, "|%-6s|", "No");
                    if (np->local == LOCAL) fprintf(myStdout, "%-7s|", "Local");
                    else fprintf(myStdout, "%-7s|", "Global");
                } else {
                    fprintf(myStdout, "%s:", np->name);
                    fprintf(myStdout, "%s:", (char *) np->def);
                    if (np->ro == LOCK) fprintf(myStdout, "%s:", "LOCKED");
                    else fprintf(myStdout, "%s:", "UNLOCKED");
                    if (np->local == LOCAL) fprintf(myStdout, "%s:", "LOCAL");
                    else fprintf(myStdout, "%s:", "GLOBAL");
                }
                fprintf(myStdout, "\n");
            }
        }
    }
    if(interactive) {
        fprintf(myStdout, "==============================================================|\n");
    } else {
        fprintf(myStdout, "END_SYMBOLS\n");
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
    if (save && !strcmp(save, "false")) return;

    fileName = getSymbol("START_FILE");
    if (!fileName) {
        setSymbol("START_FILE", "./start.rc", UNLOCK, LOCAL);
        fileName = getSymbol("START_FILE");
    }
    fp = fopen(fileName, "w");
    if (!fp) return;

    now = time(NULL);
    fprintf(fp, "# Save Date:%s", asctime(localtime(&now)));
    for (i = 0; i < SP_HASHSIZE; i++) {
        if (hashtab[i] != 0) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if(np->local == GLOBAL) {
                    fprintf(fp, "^set %s %s\n", np->name, (char *) np->def);
                    if (np->ro == LOCK) fprintf(fp, "^lock %s\n", np->name);
                }
            }
        }
    }
    fclose(fp);
}

int hash(char *s) {
    int hashval;
    for (hashval = 0; *s != '\0';) hashval += *s++;
    return (hashval % SP_HASHSIZE);
}

struct nlist *lookup(char *s) {
    struct nlist   *np;
    for (np = hashtab[hash(s)]; np != NULL; np = np->next)
        if (!strcmp(s, np->name)) return (np);
    return ((struct nlist *) NULL);
}

struct nlist   *install(char *name, void *def, int ro, int local) {
    struct nlist   *np;
    int             hashval;

    if ((np = lookup(name)) == NULL) {
        np = (struct nlist *) malloc(sizeof(*np));
        if (np == NULL) return (NULL);
        if ((np->name = strsave(name)) == NULL) return (NULL);
        hashval = hash(np->name);
        np->next = hashtab[hashval];
        hashtab[hashval] = np;
        np->ro = ro;
        np->local = local;
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
    p = r ? r->def : (char *) NULL;
    pthread_mutex_unlock(&hashLock);
    return (p);
}

void lockSymbol(char *name) {
    struct nlist   *np = lookup(name);
    if (np) np->ro = LOCK;
}

void setSymbol(char *name, void *value, int ro, int local) {
    pthread_mutex_lock(&hashLock);
    (void) install(name, value, ro, local);
    pthread_mutex_unlock(&hashLock);
}

void spreadDisconnect() {
    char sql[BUFFSIZE];
    sqlite3 *db;
    char *dataFile;
    char *zErrMsg;
    int rc;

    SP_disconnect(Mbox);
    runFlag = 0;
    sleep(1);

    dataFile = getSymbol("DATABASE");
    rc = sqlite3_open(dataFile, &db);
    strcpy(sql, "update nodes set state = 'DISCONNECTED';");
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    sqlite3_close(db);
}

int spreadConnect() {
    int ret;
    char *server = (char *)NULL;
    char *mainServer, *altServer, *user, *group, *tmp, *action;
    static int flipFlop = 0;
    char *tstServer;
    char Private_group[MAX_GROUP_NAME];

    setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
    disconnectCache();

    mainServer = (char *) getSymbol("SERVER");
    altServer = (char *) getSymbol("ALTSERVER");
    tstServer = malloc(255);

    if(flipFlop == 0) {
        strcpy(tstServer, mainServer);
        flipFlop++;
    } else {
        strcpy(tstServer, altServer);
        flipFlop = 0;
    }

    if(altServer == (char *)NULL) altServer = mainServer;

    user = (char *) getSymbol("USER");
    group = (char *) getSymbol("GROUP");

    ret = SP_connect(tstServer, user, 0, 1, &Mbox, Private_group);
    if (ret < 0) {
        ret = SP_connect(altServer, user, 0, 1, &Mbox, Private_group);
        if (ret < 0) {
            setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
            disconnectCache();
        } else {
            setSymbol("CONNECTED", "true", UNLOCK, LOCAL);
            server = altServer;
        }
    } else {
        setSymbol("CONNECTED", "true", UNLOCK, LOCAL);
        server = mainServer;
    }

    if(server != (char *)NULL) {
        tmp = strtok(tstServer, "@");
        tmp = strtok(NULL, " ");
        setSymbol("CONNECTED_TO", tmp, UNLOCK, LOCAL);
        char scratch[255];
        sprintf(scratch, "#%s#%s", user, tmp);
        setSymbol("ME", scratch, LOCK, LOCAL);
    }

    if(ret < 0) return 0;

    SP_join(Mbox, "global");
    if(strcmp(group, "global")) SP_join(Mbox, group);

    return 1;
}

void spreadLeave(char *group) {
    char sql[BUFFSIZE];
    char *me = (char *)getSymbol("ME");
    char *dataFile = getSymbol("DATABASE");
    int rows, columns;
    char **results = NULL;
    int i, rc;
    sqlite3 *db;
    char *zErrMsg = NULL;

    rc = sqlite3_open(dataFile, &db);
    if(group == (char *)NULL) {
        sprintf(sql, "select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group <> 'global';", me);
    } else {
        sprintf(sql, "select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group='%s';", me, group);
    }

    rc = sqlite3_get_table(db, sql, &results, &rows, &columns, &zErrMsg);
    if(rc == SQLITE_OK) {
        for(i = 1; i <= rows; i++) {
            SP_leave(Mbox, results[i]);
            sprintf(sql, "update nodes set state='LEAVE' where name = '%s' and sp_group='%s'", me, results[i]);
            sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        }
        sqlite3_free_table(results);
    }
    sqlite3_close(db);
}

int spreadJoin(char *group) {
    char *connected = (char *) getSymbol("CONNECTED");
    if (!strcmp(connected, "false")) return 1;
    SP_join(Mbox, group);
    return 0;
}

void rxSet(char *name, char *value) {
    char *client = getSymbol("CLIENT");
    char *fmt = getSymbol("SET_FMT");
    char *cr = getSymbol("ADD_CR");

    if(!strcmp(client, "raw")) {
        fprintf(myStdout, fmt, name, value);
    } else if(!strcmp(client, "redis-cli")) {
        fprintf(myStdout, "set %s %s\n", name, value);
    } else {
        fprintf(myStdout, "%s %s\n", name, value);
    }
    if(!strcmp(cr, "true")) fprintf(myStdout, "\n");
    fflush(myStdout);
}

void rxExit() {
    if(!strcmp(getSymbol("EXIT_COMMAND"), "true")) {
        fprintf(myStdout, "stop\n");
        fprintf(debugOut, "exit from unixToSpread.\n");
        fflush(debugOut);
        exit(0);
    }
}

void spreadRX() {
    int service_type = 0;
    int ret;
    char sender[MAX_GROUP_NAME];
    int num_groups;
    char target_groups[100][MAX_GROUP_NAME];
    int16 mess_type;
    static char message[MAX_MESSLEN];
    int endian_mismatch;
    char *me, *debug, *ign;
    char buffer[BUFFSIZE];
    char *cmd, *p1, *p2;
    int flag = 0, connected = 0;
    membership_info memb_info;

    runFlag = 1;
    while(1) {
        do {
            connected = spreadConnect();
            if(connected == 0) sleep(5);
        } while (connected == 0);

        me = getSymbol("ME");
        ign = getSymbol("IGN_GLOBAL");
        runFlag = 1;

        while (runFlag) {
            do {
                memset(message, 0, sizeof(message));
                ret = SP_receive(Mbox, &service_type, sender, 100,
                        &num_groups, target_groups,
                        &mess_type, &endian_mismatch, sizeof(message), message);
                if(ret < 0) {
                    if(ret == NET_ERROR_ON_SESSION) runFlag = 0;
                    break;
                }
            } while((!strcmp(sender, me)) && (!strcmp(getSymbol("OWN_MESSAGES"), "false")));

            debug = getSymbol("DEBUG");
            int debugFlag = strcmp(debug, "false") != 0;

            flag = (!strcmp(ign, "false")) || ((num_groups == 1) && (!strcmp(ign, "true")) && (!strcmp(target_groups[0], me)));

            if(debugFlag) {
                fprintf(debugOut, "R:%s\n", message);
            } else {
                fprintf(myStdout, "%s", message);
                fflush(myStdout);
            }

            if (Is_regular_mess(service_type)) {
                message[ret] = 0;
                if (flag != 0) {
                    strcpy(buffer, message);
                    cmd = strtok(buffer, " \n");
                    p1 = strtok(NULL, " \n");
                    p2 = strtok(NULL, " \n");

                    if (message[0] == '^') {
                        if(!strcmp(cmd, "^set")) rxSet(p1, p2);
                        else if(!strcmp(cmd, "^exit")) rxExit();
                        else if(!strcmp(cmd, "^empty")) emptyCache(p1);
                        else if(!strcmp(cmd, "^get")) {
                            p2 = getSymbol(p1);
                            if(!p2) p2 = "NOT SET";
                            SP_multicast(Mbox, AGREED_MESS, sender, 1, strlen(p2), p2);
                        }
                    } else if(!strcmp(getSymbol("PASSTHROUGH"), "true")) {
                        fprintf(myStdout, "%s", message);
                    }
                    fflush(myStdout);
                }
            } else if (Is_membership_mess(service_type)) {
                SP_get_memb_info(message, service_type, &memb_info);
                if (Is_reg_memb_mess(service_type)) {
                    if (Is_caused_join_mess(service_type)) {
                        for (int i = 0; i < num_groups; i++) {
                            if(!lookupCacheGroup(&target_groups[i][0], sender)) {
                                cacheAdd(&target_groups[i][0], sender, CONNECTED);
                            }
                            cacheSetConnected(&target_groups[i][0], sender, CONNECTED);
                        }
                    } else if (Is_caused_leave_mess(service_type)) {
                        cacheSetConnected(memb_info.changed_member, sender, LEAVE);
                    } else if (Is_caused_disconnect_mess(service_type)) {
                        cacheSetConnected(memb_info.changed_member, sender, DISCONNECTED);
                    }
                }
            }
        }
        setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
    }
}

void startSpreadRX() {
    pthread_t spread;
    lockSymbol("USER");
    pthread_create(&spread, NULL, (void *) spreadRX, (void *) NULL);
    setSymbol("CONNECTED", "true", UNLOCK, LOCAL);
}

int main(int argc, const char *argv[]) {
    char buffer[BUFFSIZE];
    char *status;
    char *cmd, *p1, *p2, *p3;
    char *tmp;
    int runFlag = 1;
    int fromFile = 0;
    int ch, uid;
    struct passwd *userRecord;
    int dump_config = 0;
    FILE *fp = NULL;
    char safeBuffer[BUFFSIZE];
    char *socketPath = NULL;

    debugOut = stderr;
    myStdin = stdin;
    myStdout = stdout;

    while((ch = getopt(argc, (char **)argv, "hc:u:d:fg:l:n:s:P")) != -1) {
        switch(ch) {
            case 'h': usage(); exit(0); break;
            case 'c': setSymbol("START_FILE", (char *) optarg, LOCK, LOCAL); break;
            case 'd': setSymbol("DATABASE", (char *) optarg, LOCK, GLOBAL); break;
            case 'u': setSymbol("USER", (char *) optarg, LOCK, LOCAL); break;
            case 'n': setSymbol("HOSTNAME", (char *) optarg, LOCK, LOCAL); break;
            case 'f': tmp = getSymbol("START_FILE"); unlink(tmp); break;
            case 'g': setSymbol("GROUP", (char *) optarg, LOCK, GLOBAL); break;
            case 'l': 
                debugOut = fopen(optarg, "a");
                if(!debugOut) debugOut = stderr;
                else {
                    setSymbol("DEBUG", "true", LOCK, GLOBAL);
                    setSymbol("LOG_FILE", (char *)optarg, LOCK, LOCAL);
                }
                break;
            case 's': socketPath = strsave(optarg); break;
            case 'P': dump_config = 1; break;
        }
    }

    if (!socketPath) {
        socketPath = strsave("/tmp/unixToSpread.sock");
    }

    setSymbol("DEBUG", "false", UNLOCK, GLOBAL);
    setSymbol("SHOW_SENDER", "false", UNLOCK, GLOBAL);
    setSymbol("EXIT_COMMAND", "false", UNLOCK, GLOBAL);
    setSymbol("OWN_MESSAGES", "false", UNLOCK, GLOBAL);
    setSymbol("SAVE_ALLOWED", "true", UNLOCK, GLOBAL);
    setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
    setSymbol("CLIENT", "raw", UNLOCK, GLOBAL);
    setSymbol("AUTOCONNECT", "false", UNLOCK, GLOBAL);
    setSymbol("PASSTHROUGH", "true", UNLOCK, GLOBAL);
    setSymbol("INTERACTIVE", "false", UNLOCK, GLOBAL);
    setSymbol("BUILD", __DATE__, LOCK, LOCAL);
    setSymbol("SET_FMT", "%s %s", UNLOCK, GLOBAL);
    setSymbol("STALE_AFTER", "600", UNLOCK, GLOBAL);
    setSymbol("ADD_CR", "false", UNLOCK, GLOBAL);
    setSymbol("IGN_GLOBAL", "false", UNLOCK, GLOBAL);
    setSymbol("PROMPT", "unixToSpread>", UNLOCK, GLOBAL);

    if(!getSymbol("USER")) {
        uid = getuid();
        userRecord = getpwuid(uid);
        setSymbol("USER", userRecord->pw_name, UNLOCK, LOCAL);
    }
    if(!getSymbol("GROUP")) setSymbol("GROUP", "global", UNLOCK, GLOBAL);
    if(!getSymbol("DATABASE")) {
        sprintf(buffer, "/var/tmp/%s.db", getSymbol("USER"));
        setSymbol("DATABASE", buffer, LOCK, LOCAL);
    }
    setSymbol("SERVER", "4803", UNLOCK, GLOBAL);
    setSymbol("ALTSERVER", "localhost:4803", UNLOCK, GLOBAL);

    if(!getSymbol("START_FILE")) {
        tmp = getenv("HOME");
        sprintf(safeBuffer, "%s/.start.rc", tmp);
        setSymbol("START_FILE", safeBuffer, UNLOCK, LOCAL);
    }

    fp = fopen(getSymbol("START_FILE"), "r");
    if (fp) {
        fromFile = 1;
    } else {
        fprintf(stderr, "A startup file ($HOME/.start.rc) MUST exist.\n");
        exit(1);
    }

    if (dump_config) {
        dumpSymbols();
        exit(0);
    }

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, socketPath, sizeof(address.sun_path) - 1);

    unlink(socketPath);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        exit(1);
    }

    fprintf(debugOut, "Listening on Unix domain socket: %s\n", socketPath);
    fflush(debugOut);

    int client_fd = -1;
    socklen_t addrlen = sizeof(address);

    while (runFlag) {
        if (client_fd < 0 && !fromFile) {
            fprintf(debugOut, "Waiting for client connection on %s...\n", socketPath);
            fflush(debugOut);
            client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (client_fd < 0) {
                if (runFlag) perror("accept failed");
                continue;
            }
            fprintf(debugOut, "Client connected.\n");
            fflush(debugOut);

            myStdin = fdopen(client_fd, "r");
            myStdout = fdopen(client_fd, "w");
            if (!myStdin || !myStdout) {
                perror("fdopen failed");
                close(client_fd);
                client_fd = -1;
                continue;
            }
            fp = myStdin;
        }

        if ((status = fgets(buffer, BUFFSIZE, fp)) == 0) {
            if (fromFile) {
                fclose(fp);
                fromFile = 0;
                continue;
            } else if (fp == myStdin) {
                fclose(myStdin);
                myStdin = NULL;
                myStdout = NULL;
                client_fd = -1;
                fprintf(debugOut, "Client disconnected.\n");
                fflush(debugOut);
                continue;
            } else {
                break;
            }
        }

        tmp = strtok(buffer, "\n");
        if (tmp) {
            strcpy(safeBuffer, tmp);
            cmd = strtok(tmp, " ");
            if (cmd) {
                if (cmd[0] == '#') {
                    // comment
                } else if (cmd[0] == '^') {
                    p1 = strtok(NULL, " ");
                    p2 = strtok(NULL, " ");
                    p3 = strtok(NULL, " ");

                    if (!strcmp(cmd, "^set")) {
                        if (p1 && p2) setSymbol(p1, p2, UNLOCK, GLOBAL);
                    } else if (!strcmp(cmd, "^lock")) {
                        if(p1) lockSymbol(p1);
                    } else if (!strcmp(cmd, "^get")) {
                        if(p1) {
                            p2 = getSymbol(p1);
                            fprintf(myStdout, "%s\n", p2 ? p2 : "NOT SET");
                        }
                    } else if (!strcmp(cmd, "^join")) {
                        if(p1) spreadJoin(p1);
                    } else if (!strcmp(cmd, "^leave")) {
                        if(p1) spreadLeave(p1);
                    } else if (!strcmp(cmd, "^dump")) {
                        dumpSymbols();
                        cacheDump();
                    } else if (!strcmp(cmd, "^save")) {
                        saveSymbols();
                    } else if (!strcmp(cmd, "^connect")) {
                        spreadConnect();
                    } else if (!strcmp(cmd, "^disconnect")) {
                        spreadDisconnect();
                    } else if (!strcmp(cmd, "^exit")) {
                        char *exitCmd = getSymbol("EXIT_COMMAND");
                        if (exitCmd && !strcmp(exitCmd, "true")) {
                            runFlag = 0;
                        }
                    } else if (!strcmp(cmd, "^start")) {
                        startSpreadRX();
                    } else if (!strcmp(cmd, "^help")) {
                        fprintf(myStdout, "Commands: ^set, ^get, ^join, ^leave, ^dump, ^start, ^exit\n");
                    }
                    fflush(myStdout);
                } else if (strlen(cmd) > 0) {
                    SP_multicast(Mbox, AGREED_MESS, getSymbol("GROUP"), 1, strlen(safeBuffer), safeBuffer);
                }
            }
        }
    }

    close(server_fd);
    unlink(socketPath);
    return (EXIT_SUCCESS);
}
