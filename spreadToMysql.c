#include "sp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include <jansson.h>
#include <mariadb/mysql.h>

#define MAX_MESSLEN     102400
#define BUFFER_LEN 255

// Structs for the new JSON structure
typedef struct {
    char *name;
    char *db;
    char *user;
    char *passwd;
} DatabaseConfig;

typedef struct {
    char *name;
    char *port;
    char *default_group;
    char *user;
} SpreadConfig;

char *Interp(char *);

struct Global {
        bool connectedToMysql;
    DatabaseConfig database;
    SpreadConfig spread;
};

struct Global g;

// Helper function to extract string value from a json object
void extract_string(json_t *parent, const char *key, char **target) {
    json_t *obj = json_object_get(parent, key);
    if (json_is_string(obj)) {
        *target = strdup(json_string_value(obj));
    }
}

int load_config(const char *filename, struct Global *g) {
    json_t *root;
    json_error_t error;

    // Load the JSON file
    root = json_load_file(filename, 0, &error);
    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }

    // Extract values from the JSON object
    json_t *database_obj = json_object_get(root, "database");
    if (database_obj) {
        extract_string(database_obj, "name", &g->database.name);
        extract_string(database_obj, "db", &g->database.db);
        extract_string(database_obj, "user", &g->database.user);
        extract_string(database_obj, "passwd", &g->database.passwd);
    }

    json_t *spread_obj = json_object_get(root, "spread");
    if (spread_obj) {
        extract_string(spread_obj, "name", &g->spread.name);
        extract_string(spread_obj, "port", &g->spread.port);
        extract_string(spread_obj, "default_group", &g->spread.default_group);
        extract_string(spread_obj, "user", &g->spread.user);
    }

    json_decref(root);
    return 0;
}

char *Interp(char *cmd) {
        char *c;
        char *saveptr;
        char *token;
        char *s1=NULL;
        static char outBuffer[BUFFER_LEN];
        int result;

        (void *)memset(outBuffer,0,BUFFER_LEN);

        token = strtok_r(cmd," \n", &saveptr);

        if(cmd[0] == '^') {

            if (!strcmp(token,"^status")){
                result = sprintf(outBuffer,"\nConnected to mySql:%d\n",(int)g.connectedToMysql);
            } else if (!strcmp(token,"^ping")) {
                strcpy(outBuffer,"+pong");
            }
        } else {
            // now check if this is a valid command.
            // So:
            // GET <name> returns a string
            // SET <name> ON|OFF set in db and returns state.
            // Anything else returns -ERROR
            if(!strcmp(cmd, "GET")) {
                printf("GET\n");
                s1 = strtok_r(NULL," \n",&saveptr);
                sprintf(outBuffer,s1);
                // 
                // Now do the sql query.
                //
            } else {
                strcpy(outBuffer,"-ERROR\n");
            }
        }

        return(outBuffer);
}

void usage() {
    printf("\n");
    printf("Usage: spreadToMysql -h|? -u <user> -g <group> -s <server> -c <config file>\n");
    printf("\nDescription:\n");
    printf("\tlightSink takes messages from spread and sends them\n");
    printf("\tto stdout.\n");
    printf("\n");

    printf("\t-h|?\t\tHelp\n");
    printf("\t-c <config>\tPath to JSON.\n");
    printf("\t-f\t\tFormatted output\n");
    printf("\t-x\t\tExit. On reciept of a message exit.\n");
    printf("\t-u <user>\tConnect to spread as user.\n");
    printf("\t-g <group>\tOn connect join group.\n");
    printf("\t-s <server>\tConnect to server, e.g 4803, 4803@host.\n");
    printf("\t-c <file>\tUse JSON config file for params.\n");
    printf("\t-t n\t\tTime out after n seconds\n");
    printf("\t-p\t\tPoll\n");

    printf("\n");
    printf("The defaults are:\n");
    printf("\tlightSink -u user -s 4803@localhost \n");

    printf("\n");
}

void sig_handler(int signo) {
    if (signo == SIGALRM) {
        fprintf(stderr,"received SIGALRM\n");
    }
    exit(signo);
}


int main(int argc, char *argv[]) {
    int             ch;
    int             service_type = 0;
    int             ret;
    int             exitFlag = 0;
    int polling = 0;
    int verbose = 0;
    int formatted = 0;
    int incSender = 1;
    int incReciever = 0;
    int incType = 0;
    int incMsg = 1;
    char *configFile = NULL;

    g.connectedToMysql = false;

    char            user[32];
    char            group[80];
    char            server[255];
    static char     message[MAX_MESSLEN];

    int             endian_mismatch;
    int16           mess_type;
    int timeout=0;
    int loop=1;

    int             num_groups;
    char            sender[MAX_GROUP_NAME];
    char            Private_group[MAX_GROUP_NAME];
    char            target_groups[100][MAX_GROUP_NAME];

    mailbox         Mbox;

    strcpy(user,"user");
    strcpy(server,"4803@localhost");
    group[0] = 0;

    while ((ch = getopt(argc, argv, "c:f:lh?u:g:s:t:pvx")) != -1) {

        switch (ch) {

            case 'c':
                configFile = optarg;
                break;
            case 'f':
                formatted=1;
                {
                    int i;
                    incSender = 0;
                    incReciever = 0;
                    incType = 0;
                    incMsg = 0;

                    for (i=0;i< strlen(optarg);i++) {
                        if( optarg[i] == 'S') {
                            incSender = 1;
                        } else if (optarg[i] == 'R' ) {
                            incReciever = 1;
                        } else if (optarg[i] == 'T' ) {
                            incType = 1;
                        } else if (optarg[i] == 'M' ) {
                            incMsg = 1;
                        }
                    }
                }
                break;
            case 'g':
                strcpy(group, optarg);
                break;
            case 'h':
            case '?':
                usage();
                exit(0);
                break;
            case 'x':
                loop=0;
                break;
            case 'p':
                polling = 1;
                break;
            case 's':
                strcpy(server, optarg);
                break;
            case 't':
                timeout=atoi(optarg);
                break;
            case 'u':
                strcpy(user, optarg);
                break;
            case 'v':
                verbose = 1;
                printf("Verbose set\n");
                break;
            default:
                usage();
                break;
        }
    }

    if (configFile) {
        if (load_config(configFile, &g) != 0) {
            fprintf(stderr, "Failed to load config file: %s\n", configFile);
            return 1;
        }
        if (g.spread.port && g.spread.name) {
            sprintf(server, "%s@%s", g.spread.port, g.spread.name);
        }
        if (g.spread.default_group) {
            strcpy(group, g.spread.default_group);
        }
        if (g.spread.user) {
            strcpy(user, g.spread.user);
        }
    }

    if(verbose) {
        printf("=========\n");
        printf("MySQL Host  :%s\n",g.database.name);
        printf("MySQL db    :%s\n",g.database.db);
        printf("MySQL User  :%s\n",g.database.user);
        printf("MySQL Passwd:%s\n",g.database.passwd);
        printf("\n");
        printf("Spread Server:%s\n",g.spread.name);
//        printf("Spread Group :%s\n",g.default_group);
        printf("Spread User  :%s\n",g.spread.user );
        printf("=========\n");
    }
//        extract_string(spread_obj, "user", &g->spread.user);
    ret = SP_connect(server, g.spread.user, 0, 1, &Mbox, Private_group);

    if (ret < 0) {
        SP_error(ret);
        exit(1);
    }
    SP_join(Mbox, "global");

    if (strlen(group) > 0 ) {
        SP_join(Mbox, group);
    }

    if(polling) {
        ret = SP_poll( Mbox);
        if(ret <= 0)
            exit(0);
    }

    do {
        if(timeout > 0) {
            alarm(timeout);
            //
            // TODO Make this s bit smarter.  Only run it once.
            //
            signal(SIGALRM, sig_handler);
        }

        ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(message), message);

        if (ret < 0) {
            SP_error(ret);
            exit(1);
        }

        if (Is_regular_mess(service_type)) {
            message[ret] = 0;

        char *t = Interp(message);
//            printf("%s\n",t);

            if(strcmp(sender, Private_group)) {
                ret = SP_multicast(Mbox,AGREED_MESS, group, 1, strlen(t),t);
            }

            if(loop ==0) {
                exitFlag = 1;
            } else {
                exitFlag=0;
            }
        }
    } while (!exitFlag);

    ret = SP_leave(Mbox, group);

    exit(0);

}
