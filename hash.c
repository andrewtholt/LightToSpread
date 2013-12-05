
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "hash.h"


static struct nlist *hashtab[HASHSIZE];
extern struct globalDefinitions global;


int hash(char *s) {
    int             hashval;

    for (hashval = 0; *s != '\0';)
        hashval += *s++;
    return (hashval % HASHSIZE);
}

struct nlist   *lookup(char *s) {
    struct nlist   *np;

    for (np = hashtab[hash(s)]; np != NULL; np = np->next)
        if (!strcmp(s, np->name))
            return (np);
    return ((struct nlist *) NULL);
}

int exists(char *key) {
    struct nlist *np;
    
    np=lookup(key);
    if(!np) {
        return(0);
    } else {
        return(1);
    }
}

struct nlist *install (char *name, void *def, int ro, int local) {
    struct nlist *np;
    int hashval;
    int len;

    if ((np = lookup (name)) == NULL) {
        symbolCount++;
        np = (struct nlist *) malloc (sizeof (*np));
        if (np == NULL) {
            return (NULL);
        }

        if ((np->name = (char *)strsave (name)) == NULL) {
            return (NULL);
        }

        hashval = hash (np->name);
        np->next = hashtab[hashval];
        hashtab[hashval] = np;
        np->ro = ro;
        np->local = local;
        //        np->def = (char *) strsave (def);
        np->value = (struct cString *)malloc(sizeof(struct cString));
        bzero(np->value->text,BUFFSIZE);
        
        len=strlen(def);
        np->value->length=len;
        
        if(len > 0) {
            strncpy(np->value->text,def,len);
        }
    }
    else {
        if (np->ro == UNLOCK) {
            free (np->value);
            np->value = (struct cString *)malloc(sizeof(struct cString));
            len=strlen(def);
            np->value->length=len;
            bzero(np->value->text,BUFFSIZE);
            strncpy(np->value->text,def,len);
        }
    }
    return (np);
}


struct cString *getSymbol(char *name) {
    struct cString *p;
    struct nlist   *r;
    pthread_mutex_lock(&hashLock);

    r = lookup(name);
    if (r) {
        p = r->value;
    } else {
        p = (struct cString *) NULL;
    }

    pthread_mutex_unlock(&hashLock);
    return (p);
}

void lockSymbol(char *name) {
    struct nlist   *np;

    np = lookup(name);

    if (np)
        np->ro = LOCK;
}

void mkLocal(char *name) {
    struct nlist   *np;

    np = lookup(name);

    np->local = LOCAL;
}

void mkGlobal(char *name) {
    struct nlist   *np;

    np = lookup(name);

    np->local = GLOBAL;
    
}


void setSymbol(char *name, void *value, int ro,int local) {
    pthread_mutex_lock(&hashLock);
    (void) install(name, value, ro, local);
    pthread_mutex_unlock(&hashLock);

}

void dumpInteractive () {
    char fmt[BUFFSIZE];
    int i;
    struct nlist *np;

    fprintf (stdout,
            "==============================================================|\n");
    fprintf (stdout, "|\tBuild Date:%s\n", getFiclParam ("BUILD"));
    fprintf (stdout,
            "==============================================================|\n");

    strcpy (fmt, "|%-20s|%-25s|%-6s|%-7s|\n");
    fprintf (stdout, fmt, "Name", "Value", "Locked", "Scope");
    fprintf (stdout,
            "==============================================================|\n");

    for (i = 0; i < HASHSIZE; i++) {
        if (hashtab[i] != 0) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                fprintf (stdout, "|%-20s", np->name);
                fprintf (stdout, "|%-25s", (char *) np->value->text);

                if (np->ro == LOCK)
                    fprintf (stdout, "|%-6s|", "Yes");
                else
                    fprintf (stdout, "|%-6s|", "No");

                if (np->local == LOCAL)
                    fprintf (stdout, "%-7s|\n", "Local");
                else
                    fprintf (stdout, "%-7s|\n", "Global");
            }
        }
    }
    fprintf (stdout,
            "|=============================================================|\n");
    fflush (stdout);
}


void dumpSymbols () {
    char *tmp;
    char *format;

    int interactive = 0;

    /*
       tmp = getSymbol ("INTERACTIVE");
       format = getSymbol ("FORMAT");

       if (!tmp) {
       interactive = 0;
       }
       else {
       interactive = (strcmp (tmp, "false") != 0);
       }
     */


    dumpInteractive ();
    /*
       if (interactive) {
       dumpInteractive ();
       }
       else {
       if (!strcmp (format, "csv")) {
       dumpCsv ();
       }
       if (!strcmp (format, "redis")) {
       dumpRedis ();
       }
       }
     */
}

void saveParam(FILE *fp,struct nlist *np) {
    fprintf (fp, "^set %s ", np->name);
    fprintf (fp, "%s", (char *) np->value->text);

    fprintf (fp, "\n");

    if (np->ro == LOCK) {
        fprintf (fp, "^lock %s\n", np->name);
    }
}

void saveSymbols () {
    struct nlist *np;
    int i;
    char *fileName;
    char *save;
    FILE *fp;
    time_t now;

    save = getFiclParam ("SAVE_ALLOWED");

    if (!save) {
        setFiclParam("SAVE_ALLOWED","true");
    } else {
        if (!strcmp (save, "false")) {
            return;
        }
    }

    fileName = getFiclParam ("START_FILE");
    if (!fileName) {
        setSymbol ("START_FILE", "./start.rc", UNLOCK, LOCAL);
        fileName = getFiclParam ("START_FILE");
    }
    
    fp = fopen (fileName, "w");

    if (!fp) {
        perror("Save File:");
        return;
    }

    now = time (NULL);

    fprintf (fp, "#\n");
    fprintf (fp, "# Save Date:%s", asctime (localtime (&now)));
    fprintf (fp, "#\n");
    fprintf (fp, "# Startup file.\n");
    fprintf (fp, "# Comments MUST be at the start of a line.\n");
    fprintf (fp, "# Any comments added will be lost.\n");
    fprintf (fp, "#\n");
    for (i = 0; i < HASHSIZE; i++) {
        if (hashtab[i] != 0) {
            for (np = hashtab[i]; np != NULL; np = np->next) {
                if (np->local == GLOBAL) {
                //
                // Convert this to a procedure
                // e.g.
                //
                if(strcmp(np->name,"MODE") ) {
                    saveParam(fp,np);
                }
                //
//                     fprintf (fp, "^set %s ", np->name);
//                     fprintf (fp, "%s", (char *) np->value->text);
// 
//                     fprintf (fp, "\n");
// 
//                     if (np->ro == LOCK) {
//                         fprintf (fp, "^lock %s\n", np->name);
//                     }
                }
            }
        }
    }
    np=lookup("MODE");
    saveParam(fp,np);
    fclose (fp);
}

void loadSymbols() {
    FILE *fp;
    char *fileName;
    char *ptr;
    int debug=0;
    char buffer[BUFFSIZE];
    int i=0;

    fileName = getFiclParam ("START_FILE");
    ptr=getFiclParam("DEBUG");

    if(!strcmp(ptr,"true")) {
        debug=1;
        printf("Load file %s\n",fileName);
    } else {
        debug=0;
    }

    fp = fopen (fileName, "r");

    if(fp != (FILE *)NULL ) {
        while( fgets(buffer,BUFFSIZE,fp)) {
            if(debug) {
                printf("%3d:%s",i,buffer);
            }
            cmdInterp(1,buffer);
            i++;
        }
        fclose(fp);
    }
}
