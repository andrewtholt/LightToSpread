/*
 * commonToSpread.c - Common implementation library for toSpread daemons
 */

#include "commonToSpread.h"

FILE *debugOut = NULL;
FILE *myStdin = NULL;
FILE *myStdout = NULL;
FILE *myStderr = NULL;

pthread_mutex_t cacheLock = PTHREAD_MUTEX_INITIALIZER;
mailbox Mbox;
int runFlag = 1;
char progName[64] = "Spread";
struct globalDefinitions global;

static struct nlist *hashtab[SP_HASHSIZE];

char *strsave(const char *s) {
    char *p;
    if (!s) return NULL;
    p = malloc(strlen(s) + 1);
    if (p) {
        strcpy(p, s);
    }
    return p;
}

int hash(char *s) {
    int hashval;
    for (hashval = 0; *s != '\0';)
        hashval += *s++;
    return (hashval % SP_HASHSIZE);
}

struct nlist *lookup(char *s) {
    struct nlist *np;
    if (!s) return NULL;
    for (np = hashtab[hash(s)]; np != NULL; np = np->next) {
        if (strcmp(s, np->name) == 0)
            return np;
    }
    return NULL;
}

struct nlist *install(char *name, void *def, int ro, int local) {
    struct nlist *np;
    unsigned int hashval;

    if (!name) return NULL;

    if ((np = lookup(name)) == NULL) {
        np = (struct nlist *)malloc(sizeof(*np));
        if (np == NULL || (np->name = strsave(name)) == NULL)
            return NULL;
        hashval = hash(name);
        np->next = hashtab[hashval];
        hashtab[hashval] = np;
    } else {
        free(np->def);
    }

    if ((np->def = strsave((char *)def)) == NULL)
        return NULL;
    np->ro = ro;
    np->local = local;
    return np;
}

char *getSymbol(char *name) {
    struct nlist *ptr;
    ptr = lookup(name);
    if (!ptr) return NULL;
    return (char *)ptr->def;
}

void lockSymbol(char *name) {
    struct nlist *ptr;
    ptr = lookup(name);
    if (ptr) ptr->ro = LOCK;
}

void setSymbol(char *name, void *value, int ro, int local) {
    struct nlist *ptr;
    ptr = lookup(name);
    if (ptr == NULL) {
        install(name, value, ro, local);
    } else {
        if (ptr->ro != LOCK) {
            install(name, value, ro, local);
        }
    }
}

void dumpSymbols(void) {
    struct nlist *np;
    int i;
    char fmt[BUFFSIZE];
    char *tmp;
    int interactive = 0;

    if (!myStdout) return;

    tmp = getSymbol("INTERACTIVE");
    if (tmp) {
        interactive = (strcmp(tmp, "false") != 0);
    }

    if (interactive) {
        fprintf(myStdout, "==============================================================|\n");
        fprintf(myStdout, "|\tBuild Date:%s\n", getSymbol("BUILD") ? getSymbol("BUILD") : "UNKNOWN");
        fprintf(myStdout, "==============================================================|\n");

        strcpy(fmt, "|%-20s|%-25s|%-6s|%-7s|\n");
        fprintf(myStdout, fmt, "Name", "Value", "Locked", "Scope");
        fprintf(myStdout, "==============================================================|\n");
    } else {
        fprintf(myStdout, "START_SYMBOLS\n");
    }

    for (i = 0; i < SP_HASHSIZE; i++) {
        if (hashtab[i] != NULL) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if (interactive) {
                    fprintf(myStdout, "|%-20s|%-25s|%-6s|%-7s|\n",
                            np->name,
                            (char *)np->def,
                            (np->ro == LOCK) ? "Yes" : "No",
                            (np->local == LOCAL) ? "Local" : "Global");
                } else {
                    fprintf(myStdout, "%s:%s:%s:%s:\n",
                            np->name,
                            (char *)np->def,
                            (np->ro == LOCK) ? "LOCKED" : "UNLOCKED",
                            (np->local == LOCAL) ? "LOCAL" : "GLOBAL");
                }
            }
        }
    }

    if (interactive) {
        fprintf(myStdout, "==============================================================|\n");
    } else {
        fprintf(myStdout, "END_SYMBOLS\n");
    }
    fflush(myStdout);
}

void saveSymbols(void) {
    struct nlist *np;
    int i;
    char *fileName;
    char *save;
    FILE *fp;
    time_t now;

    save = getSymbol("SAVE_ALLOWED");
    if (save && !strcmp(save, "false")) {
        return;
    }

    fileName = getSymbol("START_FILE");
    if (!fileName) {
        setSymbol("START_FILE", "./start.rc", UNLOCK, LOCAL);
        fileName = getSymbol("START_FILE");
    }
    fp = fopen(fileName, "w");
    if (!fp) return;

    now = time(NULL);
    fprintf(fp, "#\n");
    fprintf(fp, "# Save Date:%s", asctime(localtime(&now)));
    fprintf(fp, "#\n");
    fprintf(fp, "# %s startup file.\n", progName);
    fprintf(fp, "# Comments MUST be at the start of a line.\n");
    fprintf(fp, "# Any comments added will be lost.\n");
    fprintf(fp, "#\n");

    for (i = 0; i < SP_HASHSIZE; i++) {
        if (hashtab[i] != NULL) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if (np->local == GLOBAL) {
                    fprintf(fp, "^set %s %s\n", np->name, (char *)np->def);
                    if (np->ro == LOCK) {
                        fprintf(fp, "^lock %s\n", np->name);
                    }
                }
            }
        }
    }
    fclose(fp);
}

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    (void)NotUsed;
    if (!myStdout) return 0;
    for (int i = 0; i < argc; i++) {
        fprintf(myStdout, "%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    fprintf(myStdout, "\n");
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
    int cnt = 0;

    if (!name) return 0;
    dbName = getSymbol("DATABASE");
    if (!dbName) return 0;

    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) return 0;

    snprintf(sql, sizeof(sql), "select count(*) from nodes where name='%s';", name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "lookupCache fatal DB error: %s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
        sqlite3_close(mydb);
        exit(-1);
    }
    if (results && results[columns]) {
        cnt = atoi(results[columns]);
    }
    sqlite3_free_table(results);
    sqlite3_close(mydb);
    return cnt;
}

int lookupCacheGroup(char *name, char *group) {
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows, columns;
    char *zErrMsg;
    int cnt = 0;

    if (!name || !group) return 0;
    dbName = getSymbol("DATABASE");
    if (!dbName) return 0;

    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) return 0;

    snprintf(sql, sizeof(sql), "select count(*) from nodes where sp_group='%s' and name='%s';", group, name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "lookupCacheGroup fatal DB error: %s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
        sqlite3_close(mydb);
        exit(-1);
    }
    if (results && results[columns]) {
        cnt = atoi(results[columns]);
    }
    sqlite3_free_table(results);
    sqlite3_close(mydb);
    return cnt;
}

void cacheTouch(char *name) {
    sqlite3 *mydb;
    char *dbName;
    char sql[BUFFSIZE];
    char *zErrMsg = NULL;
    int rc;
    time_t now = time(NULL);

    if (!name) return;
    dbName = getSymbol("DATABASE");
    if (!dbName) return;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&cacheLock);
        return;
    }

    snprintf(sql, sizeof(sql), "update nodes set touched=%d where name='%s';", (int)now, name);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "cacheTouch:UPDATE:%s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

void cacheClean(void) {
    sqlite3 *mydb;
    char *dbName;
    char sql[BUFFSIZE];
    char *zErrMsg = NULL;
    int rc;
    time_t now = time(NULL);
    char *purgeAfter;

    dbName = getSymbol("DATABASE");
    if (!dbName) return;
    purgeAfter = getSymbol("STALE_AFTER");
    (void)purgeAfter;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&cacheLock);
        return;
    }

    snprintf(sql, sizeof(sql), "delete from nodes where state <> 'CONNECTED' and stale < %d;", (int)now);
    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "cacheClean:DELETE:%s\n", zErrMsg ? zErrMsg : "unknown");
            fprintf(debugOut, "%s\n", sqlite3_errmsg(mydb));
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

void cacheSetConnected(char *name, char *group, int c) {
    char *dbName;
    int rc = 0;
    char sql[BUFFSIZE];
    char tmp[16];
    char *zErrMsg = NULL;
    char *t1;
    sqlite3 *mydb;
    time_t now, stale;

    if (!name || !group) return;

    now = time(NULL);
    dbName = getSymbol("DATABASE");
    if (!dbName) return;

    t1 = getSymbol("STALE_AFTER");
    stale = now + (t1 ? atoi(t1) : 600);

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&cacheLock);
        return;
    }

    switch (c) {
        case CONNECTED:
            strcpy(tmp, "CONNECTED");
            break;
        case DISCONNECTED:
            strcpy(tmp, "DISCONNECTED");
            break;
        case LEAVE:
            strcpy(tmp, "LEAVE");
            break;
        default:
            strcpy(tmp, "UNKNOWN");
            break;
    }

    snprintf(sql, sizeof(sql), "update nodes set state='%s', touched=%d, stale=%d where name='%s' and sp_group='%s';",
             tmp, (int)now, (int)stale, name, group);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "cacheSetConnected:UPDATE:%s\n", zErrMsg ? zErrMsg : "unknown");
            fprintf(debugOut, "%s\n", sqlite3_errmsg(mydb));
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    }

    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

int isConnected(char *name) {
    char sql[BUFFSIZE];
    sqlite3 *mydb;
    char *dbName;
    int rc;
    char **results = NULL;
    int rows, columns;
    char *zErrMsg;
    int cnt = 0;

    if (!name) return 0;
    dbName = getSymbol("DATABASE");
    if (!dbName) return 0;

    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) return 0;

    snprintf(sql, sizeof(sql), "select count(*) from nodes where name='%s' and state = 'CONNECTED';", name);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "isConnected fatal DB error: %s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
        sqlite3_close(mydb);
        exit(-1);
    }
    if (results && results[columns]) {
        cnt = atoi(results[columns]);
    }
    sqlite3_free_table(results);
    sqlite3_close(mydb);
    return cnt;
}

void cacheDump(void) {
    sqlite3 *mydb;
    char *dbName;
    char *sql;
    char *zErrMsg;
    int rc;
    char **results = NULL;
    int rows, columns;
    int i, j;
    time_t clock;
    char *tmp;
    int interactive = 0;

    if (!myStdout) return;

    tmp = getSymbol("INTERACTIVE");
    if (tmp) {
        interactive = (strcmp(tmp, "false") != 0);
    }

    dbName = getSymbol("DATABASE");
    if (!dbName) return;

    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "DBERROR:cacheDump:cannot open db %s\n", dbName);
            fflush(debugOut);
        }
        return;
    }

    sql = "select name, sp_group, state, touched from nodes order by name, sp_group, touched;";
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);

    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "DBERROR:cacheDump:%s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    } else {
        if (interactive) {
            fprintf(myStdout, "================================================================================\n");
            fprintf(myStdout, "|Name                     |Group          |State       |Touched\n");
            fprintf(myStdout, "================================================================================\n");
        } else {
            fprintf(myStdout, "START_CACHE\n");
        }
        for (i = 1; i <= rows; i++) {
            for (j = 0; j < columns; j++) {
                char *val = results[(i * columns) + j];
                switch (j) {
                    case 0:
                        if (interactive) fprintf(myStdout, "|%-25s|", val ? val : "");
                        else fprintf(myStdout, "%s:", val ? val : "");
                        break;
                    case 1:
                        if (interactive) fprintf(myStdout, "%-15s|", val ? val : "");
                        else fprintf(myStdout, "%s:", val ? val : "");
                        break;
                    case 2:
                        if (interactive) fprintf(myStdout, "%-12s|", val ? val : "");
                        else fprintf(myStdout, "%s:", val ? val : "");
                        break;
                    case 3:
                        clock = (time_t)(val ? atoi(val) : 0);
                        if (interactive) fprintf(myStdout, "%s", asctime(localtime(&clock)));
                        else fprintf(myStdout, "%d:\n", (int)clock);
                        break;
                    default:
                        if (interactive) fprintf(myStdout, "%-5s|", val ? val : "");
                        break;
                }
            }
        }
        if (interactive) {
            fprintf(myStdout, "================================================================================\n");
        } else {
            fprintf(myStdout, "END_CACHE\n");
        }
        sqlite3_free_table(results);
    }
    sqlite3_close(mydb);
    fflush(myStdout);
}

void disconnectCache(void) {
    char sql[BUFFSIZE];
    sqlite3 *db;
    char *dataFile;
    char *zErrMsg = NULL;
    int rc;

    snprintf(sql, sizeof(sql), "update nodes set state = 'DISCONNECTED';");
    dataFile = getSymbol("DATABASE");
    if (!dataFile) return;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dataFile, &db);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
        if (rc != SQLITE_OK) {
            if (debugOut) {
                fprintf(debugOut, "disconnectCache:UPDATE:%s\n", zErrMsg ? zErrMsg : "unknown");
                fflush(debugOut);
            }
            if (zErrMsg) sqlite3_free(zErrMsg);
        }
        sqlite3_close(db);
    }
    pthread_mutex_unlock(&cacheLock);
}

void emptyCache(char *all) {
    sqlite3 *mydb;
    char *dbName;
    char *me;
    int rc;
    char *zErrMsg = NULL;
    char sql[BUFFSIZE];
    time_t now = time(NULL);

    me = getSymbol("ME");
    dbName = getSymbol("DATABASE");
    if (!dbName) return;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&cacheLock);
        return;
    }

    if (all == NULL) {
        snprintf(sql, sizeof(sql), "delete from nodes where state = 'DISCONNECTED' and name <> '%s';", me ? me : "");
    } else if (!strcmp(all, "all")) {
        snprintf(sql, sizeof(sql), "delete from nodes where state <> 'CONNECTED';");
    } else if (!strcmp(all, "stale")) {
        snprintf(sql, sizeof(sql), "delete from nodes where state <> 'CONNECTED' and stale < %d;", (int)now);
    } else {
        snprintf(sql, sizeof(sql), "delete from nodes where state = 'DISCONNECTED';");
    }

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "emptyCache:DELETE:%s\n", zErrMsg ? zErrMsg : "unknown");
            fprintf(debugOut, "%s\n", sqlite3_errmsg(mydb));
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    }
    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

void cacheRemove(char *n, char *g) {
    char *dbName;
    char sql[BUFFSIZE];
    char scratch[BUFFSIZE];
    char *zErrMsg = NULL;
    sqlite3 *mydb;
    int rc;

    if (!n) return;
    dbName = getSymbol("DATABASE");
    if (!dbName) return;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&cacheLock);
        return;
    }

    snprintf(sql, sizeof(sql), "delete from nodes where name='%s'", n);
    if (g) {
        snprintf(scratch, sizeof(scratch), " and sp_group='%s';", g);
    } else {
        strcpy(scratch, ";");
    }
    strncat(sql, scratch, sizeof(sql) - strlen(sql) - 1);

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "cacheRemove:DELETE:%s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
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
    char *zErrMsg = NULL;
    time_t now = time(NULL);

    if (!n || !g) return;

    dbName = getSymbol("DATABASE");
    if (!dbName) return;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&cacheLock);
        return;
    }

    snprintf(sql, sizeof(sql), "select count(*) from nodes where name='%s' and sp_group='%s';", n, g);
    rc = sqlite3_get_table(mydb, sql, &results, &rows, &columns, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "DBERROR:%s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    } else {
        if (results && results[columns]) {
            cnt = atoi(results[columns]);
        }
        sqlite3_free_table(results);
    }

    switch (connect) {
        case CONNECTED:
            strcpy(status, "CONNECTED");
            break;
        case DISCONNECTED:
            strcpy(status, "DISCONNECTED");
            break;
        case LEAVE:
            strcpy(status, "LEAVE");
            break;
        default:
            strcpy(status, "UNKNOWN");
            break;
    }

    if (cnt == 0) {
        snprintf(sql, sizeof(sql), "insert into nodes (name, sp_group, state, touched) values ('%s','%s','%s',%d);",
                 n, g, status, (int)now);
    } else {
        snprintf(sql, sizeof(sql), "update nodes set state = '%s', touched=%d where name='%s' and sp_group='%s';",
                 status, (int)now, n, g);
    }

    rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "cacheAdd:INSERT/UPDATE:%s\n", zErrMsg ? zErrMsg : "unknown");
            fprintf(debugOut, "%s\n", sqlite3_errmsg(mydb));
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    }

    sqlite3_close(mydb);
    pthread_mutex_unlock(&cacheLock);
}

int execSQL(char *sql) {
    char *zErrMsg = NULL;
    int rc;
    sqlite3 *mydb;
    char *dbName;

    if (!sql) return 0;
    dbName = getSymbol("DATABASE");
    if (!dbName) return 0;

    pthread_mutex_lock(&cacheLock);
    rc = sqlite3_open(dbName, &mydb);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(mydb, sql, 0, 0, &zErrMsg);
        if (rc != SQLITE_OK && debugOut) {
            fprintf(debugOut, "execSQL:%s\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
        sqlite3_close(mydb);
    }
    pthread_mutex_unlock(&cacheLock);
    return rc;
}

void setupDatabase(void) {
    char *tmp;
    int dbExists = 0;
    char safeBuffer[BUFFSIZE];
    char buffer[BUFFSIZE];
    int i;
    int rc;
    sqlite3 *db;
    char *zErrMsg = NULL;

    tmp = getSymbol("DATABASE");
    if (!tmp) {
        strcpy(safeBuffer, "/tmp/nb2spread.db");
        tmp = safeBuffer;
        setSymbol("DATABASE", tmp, UNLOCK, GLOBAL);
        lockSymbol("DATABASE");
    }

    if (!strcmp(tmp, ":memory:")) {
        dbExists = 0;
    } else {
        i = open(tmp, O_RDONLY);
        if (i >= 0) {
            if (debugOut) {
                fprintf(debugOut, "Database %s exists\n", tmp);
                fflush(debugOut);
            }
            dbExists = 1;
            close(i);
        } else {
            dbExists = 0;
        }
    }

    rc = sqlite3_open(tmp, &db);
    if (rc != SQLITE_OK) {
        if (debugOut) {
            fprintf(debugOut, "Cannot open database %s: %s\n", tmp, sqlite3_errmsg(db));
            fflush(debugOut);
        }
        return;
    }

    if (dbExists) {
        if (debugOut) {
            fprintf(debugOut, "DB existed at start, assume tables defined.\n");
            fflush(debugOut);
        }
        strcpy(buffer, "update nodes set state='DISCONNECTED';");
        rc = sqlite3_exec(db, buffer, callback, 0, &zErrMsg);
        if (zErrMsg) sqlite3_free(zErrMsg);
    } else {
        if (debugOut) {
            fprintf(debugOut, "New DB, define tables\n");
            fflush(debugOut);
        }
        strcpy(buffer, "create table if not exists nodes ( sp_group varchar(32), name varchar(32), state varchar(8) default 'UNKNOWN', touched integer, stale integer, flag integer default 0);");
        rc = sqlite3_exec(db, buffer, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK && debugOut) {
            fprintf(debugOut, "Error on CREATE nodes: %s.\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);

        strcpy(buffer, "create table if not exists symbol ( key varchar(32) unique, value varchar(32));");
        rc = sqlite3_exec(db, buffer, callback, 0, &zErrMsg);
        if (rc != SQLITE_OK && debugOut) {
            fprintf(debugOut, "Error on CREATE symbol: %s.\n", zErrMsg ? zErrMsg : "unknown");
            fflush(debugOut);
        }
        if (zErrMsg) sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);
}

void spreadDisconnect(void) {
    char sql[BUFFSIZE];
    sqlite3 *db;
    char *dataFile;
    char *zErrMsg = NULL;
    int rc;

    SP_disconnect(Mbox);
    runFlag = 0;
    sleep(1);

    dataFile = getSymbol("DATABASE");
    if (!dataFile) return;

    rc = sqlite3_open(dataFile, &db);
    if (rc == SQLITE_OK) {
        strcpy(sql, "update nodes set state = 'DISCONNECTED';");
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
        if (zErrMsg) sqlite3_free(zErrMsg);
        sqlite3_close(db);
    }
}

int spreadConnect(void) {
    int ret;
    char *debug;
    char scratch[255];
    char *server = NULL;
    char *mainServer;
    char *altServer;
    char *user;
    char *group;
    char *tmp;
    char *action;
    int dbg = 0;
    static int flipFlop = 0;
    char *tstServer;
    char Private_group[MAX_GROUP_NAME];

    setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
    disconnectCache();

    debug = getSymbol("DEBUG");
    dbg = (debug && !strcmp(debug, "true"));

    mainServer = getSymbol("SERVER");
    altServer = getSymbol("ALTSERVER");
    tstServer = malloc(255);
    if (!tstServer) return 0;

    if (flipFlop == 0) {
        strncpy(tstServer, mainServer ? mainServer : "4803", 254);
        flipFlop++;
    } else {
        strncpy(tstServer, altServer ? altServer : (mainServer ? mainServer : "4803"), 254);
        flipFlop = 0;
    }
    tstServer[254] = '\0';

    if (!altServer) altServer = mainServer;

    user = getSymbol("USER");
    group = getSymbol("GROUP");

    if (dbg && debugOut) {
        fprintf(debugOut, "Connecting to %s ... \n", tstServer);
        fflush(debugOut);
    }

    ret = SP_connect(tstServer, user, 0, 1, &Mbox, Private_group);
    if (ret < 0) {
        if (dbg && debugOut) {
            SP_error(ret);
            fprintf(debugOut, "Failed.\nConnecting to %s\n", altServer ? altServer : "NULL");
            fflush(debugOut);
        }
        if (altServer) {
            ret = SP_connect(altServer, user, 0, 1, &Mbox, Private_group);
        }
        if (ret < 0) {
            if (dbg && debugOut) {
                fprintf(debugOut, "SP_connect Failed.\n");
                fflush(debugOut);
            }
            setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
            disconnectCache();
        } else {
            setSymbol("CONNECTED", "true", UNLOCK, LOCAL);
            server = altServer;
        }
    } else {
        setSymbol("CONNECTED", "true", UNLOCK, LOCAL);
        server = mainServer;
        if (dbg && debugOut) {
            fprintf(debugOut, "... done\n");
            fflush(debugOut);
        }
    }

    if (server != NULL) {
        tmp = strtok(tstServer, "@");
        tmp = strtok(NULL, " ");
        if (tmp) {
            setSymbol("CONNECTED_TO", tmp, UNLOCK, LOCAL);
            snprintf(scratch, sizeof(scratch), "#%s#%s", user ? user : "", tmp);
            setSymbol("ME", scratch, LOCK, LOCAL);
        }
    }
    free(tstServer);

    if (ret < 0) return 0;

    SP_join(Mbox, "global");
    if (group && strcmp(group, "global") != 0) {
        SP_join(Mbox, group);
    }

    action = getSymbol("ONJOIN");
    if (action && myStdout) {
        fprintf(myStdout, "%s\n", action);
        fflush(myStdout);
    }

    tmp = getSymbol("ON_CONNECT");
    if (tmp && strlen(tmp) > 0 && myStdout) {
        fprintf(myStdout, "%s\n", tmp);
        fflush(myStdout);
    }

    return 1;
}

void spreadLeave(char *group) {
    char sql[BUFFSIZE];
    char *me = getSymbol("ME");
    char *dataFile = getSymbol("DATABASE");
    int rows, columns;
    char **results = NULL;
    int i, rc;
    sqlite3 *db;
    char *zErrMsg = NULL;

    if (!dataFile) return;
    rc = sqlite3_open(dataFile, &db);
    if (rc != SQLITE_OK) return;

    if (group == NULL) {
        snprintf(sql, sizeof(sql), "select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group <> 'global';", me ? me : "");
    } else {
        snprintf(sql, sizeof(sql), "select sp_group from nodes where name='%s' and state='CONNECTED' and sp_group='%s';", me ? me : "", group);
    }

    rc = sqlite3_get_table(db, sql, &results, &rows, &columns, &zErrMsg);
    if (rc == SQLITE_OK && results) {
        for (i = 1; i <= rows; i++) {
            if (results[i]) {
                SP_leave(Mbox, results[i]);
                snprintf(sql, sizeof(sql), "update nodes set state='LEAVE' where name = '%s' and sp_group='%s'", me ? me : "", results[i]);
                sqlite3_exec(db, sql, callback, 0, &zErrMsg);
                if (zErrMsg) sqlite3_free(zErrMsg);
            }
        }
        sqlite3_free_table(results);
    } else if (zErrMsg) {
        sqlite3_free(zErrMsg);
    }
    sqlite3_close(db);
}

int spreadJoin(char *group) {
    char *connected = getSymbol("CONNECTED");
    if (connected && !strcmp(connected, "false")) {
        if (debugOut) {
            fprintf(debugOut, "Not Connected\n");
            fflush(debugOut);
        }
        return 1;
    }
    if (group) {
        SP_join(Mbox, group);
    }
    return 0;
}

void rxSet(char *name, char *value) {
    char *target;
    char buffer[BUFFSIZE];

    if (!name || !value) return;

    setSymbol(name, value, UNLOCK, LOCAL);
    target = getSymbol("ON_CHANGE");
    if (target && strlen(target) > 0) {
        snprintf(buffer, sizeof(buffer), "%s %s %s", target, name, value);
        int ret = system(buffer);
        (void)ret;
    }
}

void rxExit(void) {
    if (!strcmp(getSymbol("EXIT_COMMAND"), "true")) {
        if (myStdout) fprintf(myStdout, "stop\n");
        if (debugOut) {
            fprintf(debugOut, "exit from %s.\n", progName);
            fflush(debugOut);
        }
        exit(0);
    }
}

void spreadRX(void) {
    int service_type = 0;
    int ret;
    char sender[MAX_GROUP_NAME];
    int num_groups;
    char target_groups[100][MAX_GROUP_NAME];
    int16 mess_type;
    static char message[MAX_MESSLEN];
    int endian_mismatch;

    char *me;
    char *debug;
    int debugFlag = 0;
    char buffer[BUFFSIZE];
    char *cmd, *p1, *p2;
    char *ign;
    int flag = 0;
    int connected = 0;
    int i = 0;
    membership_info memb_info;

    setupDatabase();
    runFlag = 1;

    while (1) {
        do {
            connected = spreadConnect();
            if (connected == 0) {
                debug = getSymbol("DEBUG");
                if (debug && strcmp(debug, "false") != 0) {
                    fprintf(stderr, "... waiting ...\n");
                }
                sleep(5);
            }
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
                if (ret < 0) {
                    switch (ret) {
                        case NET_ERROR_ON_SESSION:
                            runFlag = 0;
                            break;
                        case CONNECTION_CLOSED:
                            fprintf(stderr, "connection closed\n");
                            break;
                        default:
                            SP_error(ret);
                            break;
                    }
                    break;
                }
            } while ((me && !strcmp(sender, me)) && (!strcmp(getSymbol("OWN_MESSAGES"), "false")));

            if (ret < 0) {
                break;
            }

            debug = getSymbol("DEBUG");
            debugFlag = (debug && strcmp(debug, "false") != 0);

            flag = (!ign || !strcmp(ign, "false")) ||
                   ((num_groups == 1) && (!strcmp(ign, "true")) && (me && !strcmp(target_groups[0], me)));

            if (debugFlag) {
                fprintf(debugOut, "Flag = %d\n", flag);
                fprintf(debugOut, "R:%s\n", message);
                fprintf(debugOut, "Message sent to %d recipients\n", num_groups);
                for (i = 0; i < num_groups; i++) {
                    fprintf(debugOut, "Message sent to %d:%s\n", i, target_groups[i]);
                    cacheAdd((char *)&target_groups[i], sender, CONNECTED);
                }
                fprintf(debugOut, "===========>\n");
                fflush(debugOut);
            }

            if (Is_regular_mess(service_type)) {
                message[ret] = '\0';
                if (flag != 0) {
                    if (myStdout) {
                        if (!strcmp(getSymbol("SHOW_SENDER"), "true")) {
                            fprintf(myStdout, "%s ", sender);
                        }
                    }

                    strncpy(buffer, message, sizeof(buffer) - 1);
                    buffer[sizeof(buffer) - 1] = '\0';
                    cmd = strtok(buffer, " \n");
                    p1 = strtok(NULL, " \n");
                    p2 = strtok(NULL, " \n");

                    if (message[0] == '^') {
                        if (!strcmp(cmd, "^set")) {
                            rxSet(p1, p2);
                        } else if (!strcmp(cmd, "^exit")) {
                            rxExit();
                        } else if (!strcmp(cmd, "^empty")) {
                            emptyCache(p1);
                        } else if (!strcmp(cmd, "^get")) {
                            if (debugFlag && debugOut) {
                                fprintf(debugOut, "%s wants to Get %s\n", sender, p1);
                                fflush(debugOut);
                            }
                            p2 = getSymbol(p1);
                            if (p2 == NULL) p2 = "NOT SET";
                            SP_multicast(Mbox, AGREED_MESS, sender, 1, strlen(p2), p2);
                        }
                    } else if (!strcmp(getSymbol("PASSTHROUGH"), "true")) {
                        if (myStdout) {
                            if (!strcmp(getSymbol("ADD_CR"), "true")) {
                                p1 = strtok(message, "\n");
                                if (p1) fprintf(myStdout, "%s\n", p1);
                            } else {
                                fprintf(myStdout, "%s", message);
                            }
                        }
                    }
                    if (myStdout) fflush(myStdout);
                }
                p1 = getSymbol("AUTOPURGE");
                if (p1 && !strcmp(p1, "true")) {
                    cacheClean();
                }
            } else if (Is_membership_mess(service_type)) {
                ret = SP_get_memb_info(message, service_type, &memb_info);
                if (ret < 0) {
                    if (debugOut) {
                        fprintf(debugOut, "BUG: membership message does not have valid body\n");
                        fflush(debugOut);
                    }
                    SP_error(ret);
                    exit(1);
                }
                if (Is_reg_memb_mess(service_type)) {
                    if (debugFlag && debugOut) {
                        fprintf(debugOut, "%s:Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                                memb_info.changed_member, sender, num_groups, mess_type);
                        fflush(debugOut);
                    }
                    if (Is_caused_join_mess(service_type)) {
                        if (debugFlag && debugOut) {
                            fprintf(debugOut, "%s:JOINED group %s\n", memb_info.changed_member, sender);
                            fflush(debugOut);
                        }
                        for (i = 0; i < num_groups; i++) {
                            if (debugFlag && debugOut) {
                                fprintf(debugOut, "\t%s\n", (char *)&target_groups[i][0]);
                                fflush(debugOut);
                            }
                            if (!lookupCacheGroup(&target_groups[i][0], sender)) {
                                cacheAdd((char *)&target_groups[i][0], sender, CONNECTED);
                            }
                            cacheSetConnected((char *)&target_groups[i][0], sender, CONNECTED);
                        }
                    } else if (Is_caused_leave_mess(service_type)) {
                        if (debugFlag && debugOut) {
                            fprintf(debugOut, "%s:LEFT group %s\n", memb_info.changed_member, sender);
                            fflush(debugOut);
                        }
                        cacheSetConnected((char *)memb_info.changed_member, sender, LEAVE);
                    } else if (Is_caused_disconnect_mess(service_type)) {
                        if (debugFlag && debugOut) {
                            fprintf(debugOut, "spreadRX:%s:DISCONNECT group %s\n", memb_info.changed_member, sender);
                            fflush(debugOut);
                        }
                        cacheSetConnected((char *)memb_info.changed_member, sender, DISCONNECTED);
                    } else if (Is_caused_network_mess(service_type)) {
                        int num_vs_sets, j;
                        unsigned int my_vsset_index;
                        vs_set_info vssets[MAX_VSSETS];
                        char sql[1024];
                        char members[MAX_MEMBERS][MAX_GROUP_NAME];

                        num_vs_sets = SP_get_vs_sets_info(message, &vssets[0], MAX_VSSETS, &my_vsset_index);
                        for (i = 0; i < num_vs_sets; i++) {
                            ret = SP_get_vs_set_members(message, &vssets[i], members, MAX_MEMBERS);
                            for (j = 0; j < vssets[i].num_members; j++) {
                                snprintf(sql, sizeof(sql), "update nodes set flag=1 where name ='%s' ;", members[j]);
                                execSQL(sql);
                            }
                            snprintf(sql, sizeof(sql), "update nodes set state='UNKNOWN' where flag = 0 and sp_group ='%s';", sender);
                            execSQL(sql);
                            snprintf(sql, sizeof(sql), "update nodes set flag=0;");
                            execSQL(sql);
                        }
                    }
                    if (myStdout) fflush(myStdout);
                }
            }
        }
        setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
    }
}

void startSpreadRX(void) {
    pthread_t spread;
    lockSymbol("USER");
    pthread_create(&spread, NULL, (void *)spreadRX, (void *)NULL);
    setSymbol("CONNECTED", "true", UNLOCK, LOCAL);
}

void initDefaultSymbols(const char *progname) {
    char prompt[128];

    debugOut = stderr;
    myStdin = stdin;
    myStdout = stdout;
    myStderr = stderr;

    if (progname) {
        strncpy(progName, progname, sizeof(progName) - 1);
        progName[sizeof(progName) - 1] = '\0';
    }

    setSymbol("DEBUG", "false", UNLOCK, GLOBAL);
    setSymbol("SHOW_SENDER", "false", UNLOCK, GLOBAL);
    setSymbol("EXIT_COMMAND", "false", UNLOCK, GLOBAL);
    setSymbol("OWN_MESSAGES", "false", UNLOCK, GLOBAL);
    setSymbol("SAVE_ALLOWED", "true", UNLOCK, GLOBAL);
    setSymbol("CONNECTED", "false", UNLOCK, LOCAL);
    setSymbol("CONFIGURED", "false", UNLOCK, LOCAL);
    setSymbol("CLIENT", "raw", UNLOCK, GLOBAL);
    setSymbol("AUTOCONNECT", "false", UNLOCK, GLOBAL);
    setSymbol("PASSTHROUGH", "true", UNLOCK, GLOBAL);
    setSymbol("INTERACTIVE", "false", UNLOCK, GLOBAL);
    setSymbol("BUILD", __DATE__, LOCK, LOCAL);
    setSymbol("CLEARDB", "false", UNLOCK, GLOBAL);
    setSymbol("SET_FMT", "%s %s", UNLOCK, GLOBAL);
    setSymbol("AUTOPURGE", "false", UNLOCK, GLOBAL);
    setSymbol("STALE_AFTER", "600", UNLOCK, GLOBAL);
    setSymbol("ADD_CR", "false", UNLOCK, GLOBAL);
    setSymbol("IGN_GLOBAL", "false", UNLOCK, GLOBAL);
    setSymbol("COMMS_VARS", "false", UNLOCK, GLOBAL);
    setSymbol("REMOTE_CREATE", "false", LOCK, GLOBAL);
    setSymbol("ON_CHANGE", "", LOCK, GLOBAL);
    setSymbol("ON_CONNECT", "", UNLOCK, GLOBAL);

    snprintf(prompt, sizeof(prompt), "%s>", progName);
    setSymbol("PROMPT", prompt, UNLOCK, GLOBAL);
}

void finishSymbolInitialization(void) {
    uid_t uid;
    struct passwd *userRecord;
    char buffer[BUFFSIZE];
    char safeBuffer[BUFFSIZE];
    char *tmp;
    char *home;

    if (!getSymbol("USER")) {
        uid = getuid();
        userRecord = getpwuid(uid);
        if (userRecord) {
            setSymbol("USER", userRecord->pw_name, UNLOCK, LOCAL);
        }
    }
    if (!getSymbol("GROUP")) {
        setSymbol("GROUP", "global", UNLOCK, GLOBAL);
    }
    if (!getSymbol("DATABASE")) {
        char *user = getSymbol("USER");
        snprintf(buffer, sizeof(buffer), "/var/tmp/%s.db", user ? user : "spread");
        setSymbol("DATABASE", buffer, LOCK, LOCAL);
    }
    setSymbol("SERVER", "4803", UNLOCK, GLOBAL);
    setSymbol("ALTSERVER", "localhost:4803", UNLOCK, GLOBAL);

    if (!getSymbol("START_FILE")) {
        home = getenv("HOME");
        snprintf(safeBuffer, sizeof(safeBuffer), "%s/.start.rc", home ? home : "/tmp");
        setSymbol("START_FILE", safeBuffer, UNLOCK, LOCAL);
    }

    if (!getSymbol("HOSTNAME")) {
        if (gethostname(buffer, BUFFSIZE) == 0) {
            tmp = strtok(buffer, ".");
            if (tmp) {
                setSymbol("HOSTNAME", tmp, LOCK, LOCAL);
            }
        }
    }

    if (getSymbol("LOG_FILE")) {
        myStderr = fopen(getSymbol("LOG_FILE"), "a");
        if (!myStderr) myStderr = stderr;
    } else {
        myStderr = stderr;
    }
}

int handleCommonOption(int ch, const char *arg, int *dump_config) {
    char *tmp;
    switch (ch) {
        case 'c':
            setSymbol("START_FILE", (char *)arg, LOCK, LOCAL);
            return 1;
        case 'd':
            setSymbol("DATABASE", (char *)arg, LOCK, GLOBAL);
            return 1;
        case 'u':
            setSymbol("USER", (char *)arg, LOCK, LOCAL);
            return 1;
        case 'n':
            setSymbol("HOSTNAME", (char *)arg, LOCK, LOCAL);
            return 1;
        case 'f':
            tmp = getSymbol("START_FILE");
            if (tmp) unlink(tmp);
            return 1;
        case 'g':
            setSymbol("GROUP", (char *)arg, LOCK, GLOBAL);
            return 1;
        case 'l':
            debugOut = fopen(arg, "a");
            if (!debugOut) {
                debugOut = stderr;
            } else {
                setSymbol("DEBUG", "true", LOCK, GLOBAL);
                setSymbol("LOG_FILE", (char *)arg, LOCK, LOCAL);
            }
            return 1;
        case 'P':
            if (dump_config) *dump_config = 1;
            return 1;
        default:
            return 0;
    }
}

int processConfigFile(const char *filename) {
    FILE *fp;
    char buffer[BUFFSIZE];
    int run = 1;
    char *autoconnect;

    if (!filename) return -1;
    fp = fopen(filename, "r");
    if (!fp) return -1;

    while (fgets(buffer, BUFFSIZE, fp) != NULL) {
        processSpreadCommand(buffer, &run);
        if (!run) break;
    }
    fclose(fp);

    autoconnect = getSymbol("AUTOCONNECT");
    if (autoconnect && !strcmp(autoconnect, "true")) {
        startSpreadRX();
    }
    return 0;
}

int processSpreadCommand(char *line, int *pRunFlag) {
    char safeBuffer[BUFFSIZE];
    char *cmd, *p1, *p2, *p3;
    char *tmp;
    int ret;

    if (!line) return 1;

    tmp = strtok(line, "\r\n");
    if (!tmp) return 1;

    strncpy(safeBuffer, tmp, BUFFSIZE - 1);
    safeBuffer[BUFFSIZE - 1] = '\0';

    cmd = strtok(tmp, " ");
    if (!cmd) return 1;

    if (cmd[0] == '#') {
        /* Comment line, ignore */
        return 1;
    } else if (cmd[0] == '^') {
        p1 = strtok(NULL, " ");
        p2 = strtok(NULL, " ");
        p3 = strtok(NULL, " ");
        (void)p3;

        if (!strcmp(cmd, "^set")) {
            if (p1 && p2) {
                setSymbol(p1, p2, UNLOCK, GLOBAL);
            }
        } else if (!strcmp(cmd, "^lock")) {
            if (p1) lockSymbol(p1);
        } else if (!strcmp(cmd, "^get")) {
            if (p1) {
                p2 = getSymbol(p1);
                if (myStdout) {
                    fprintf(myStdout, "%s\n", p2 ? p2 : "NOT SET");
                }
            }
        } else if (!strcmp(cmd, "^join")) {
            if (p1) spreadJoin(p1);
        } else if (!strcmp(cmd, "^leave")) {
            if (p1) spreadLeave(p1);
        } else if (!strcmp(cmd, "^dump")) {
            if (!p1) {
                dumpSymbols();
                cacheDump();
            } else if (!strcmp(p1, "symbols")) {
                dumpSymbols();
            } else if (!strcmp(p1, "cache")) {
                cacheDump();
            }
        } else if (!strcmp(cmd, "^save")) {
            saveSymbols();
        } else if (!strcmp(cmd, "^connect")) {
            spreadConnect();
        } else if (!strcmp(cmd, "^disconnect")) {
            spreadDisconnect();
        } else if (!strcmp(cmd, "^exit")) {
            if (pRunFlag) *pRunFlag = 0;
            return 0;
        } else if (!strcmp(cmd, "^add")) {
            if (p1 && p2) cacheAdd(p1, p2, UNKNOWN);
        } else if (!strcmp(cmd, "^del")) {
            if (p1) cacheRemove(p1, p2);
        } else if (!strcmp(cmd, "^empty")) {
            emptyCache(p1);
        } else if (!strcmp(cmd, "^connected")) {
            if (myStdout) {
                if (p1 && isConnected(p1)) {
                    fprintf(myStdout, "TRUE\n");
                } else {
                    fprintf(myStdout, "FALSE\n");
                }
            }
        } else if (!strcmp(cmd, "^multicast")) {
            if (p1 && p2) {
                ret = SP_multicast(Mbox, AGREED_MESS, p1, 1, strlen(p2), p2);
                (void)ret;
            }
        } else if (!strcmp(cmd, "^exec")) {
            p1 = strtok(safeBuffer, " ");
            p1 = strtok(NULL, "");
            if (p1) {
                ret = system(p1);
                (void)ret;
            }
        } else if (!strcmp(cmd, "^start")) {
            startSpreadRX();
        } else if (!strcmp(cmd, "^send")) {
            if (p1 && p2) {
                char sendBuf[BUFFSIZE + 2];
                snprintf(sendBuf, sizeof(sendBuf), "%s\n", p2);
                ret = SP_multicast(Mbox, AGREED_MESS, p1, 1, strlen(sendBuf), sendBuf);
                (void)ret;
            }
        } else if (!strcmp(cmd, "^help")) {
            if (myStdout) {
                fprintf(myStdout, "Available commands:\n");
                fprintf(myStdout, "  ^set <param> <value>\n");
                fprintf(myStdout, "  ^lock <param>\n");
                fprintf(myStdout, "  ^get <param>\n");
                fprintf(myStdout, "  ^join <group>\n");
                fprintf(myStdout, "  ^leave <group>\n");
                fprintf(myStdout, "  ^dump [symbols|cache]\n");
                fprintf(myStdout, "  ^save\n");
                fprintf(myStdout, "  ^connect\n");
                fprintf(myStdout, "  ^disconnect\n");
                fprintf(myStdout, "  ^add <node> <grp>\n");
                fprintf(myStdout, "  ^del <node> <grp>\n");
                fprintf(myStdout, "  ^empty [all|stale]\n");
                fprintf(myStdout, "  ^connected <name>\n");
                fprintf(myStdout, "  ^multicast <group> <message>\n");
                fprintf(myStdout, "  ^exec <command>\n");
                fprintf(myStdout, "  ^start\tAction the settings and connect.\n");
                fprintf(myStdout, "  ^send <group> <message>\n");
                fprintf(myStdout, "  ^exit\n");
                fprintf(myStdout, "  ^help\n");
            }
        } else {
            if (myStdout) {
                fprintf(myStdout, "Unknown command %s\n", cmd);
            }
        }
        if (myStdout) fflush(myStdout);
    } else if (strlen(cmd) > 0) {
        char *grp = getSymbol("GROUP");
        if (grp) {
            ret = SP_multicast(Mbox, AGREED_MESS, grp, 1, strlen(safeBuffer), safeBuffer);
            (void)ret;
        }
    }

    if (myStdout) fflush(myStdout);
    return 1;
}
