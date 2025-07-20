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

char *Interp(char *, MYSQL *conn);
void print_config();

struct Global {
        bool connectedToMysql;
    DatabaseConfig database;
    SpreadConfig spread;
};
void finish_with_error(MYSQL *con) {
    fprintf(stderr,"%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}

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

char *Interp(char *cmd, MYSQL *conn) {
        char *c;
        char *saveptr;
        char *token;
        char *s1=NULL;
        char *s2=NULL;

        static char outBuffer[BUFFER_LEN];
        static char sqlCmd[BUFFER_LEN];
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
                // 
                // Now do the sql query.
                //
                sprintf(sqlCmd,"select state from  io_point where name='%s';",s1);

                strcpy(outBuffer, sqlCmd);

                MYSQL_FIELD *field;

                int rc =mysql_query(conn,sqlCmd);
                if ( rc != 0 ) {
                    finish_with_error(conn);
                }

                MYSQL_RES *result = mysql_store_result(conn);
                MYSQL_ROW row = mysql_fetch_row(result);

                if (row == 0) {
                    strcpy(outBuffer,"-EMPTY\n");
                } else {
                    printf("\t%s\n",row[0]);
                    sprintf(outBuffer,"%s\n",row[0]);
                }
            } else if(!strcmp(cmd,"SET")){
                printf("SET\n");
                s1 = strtok_r(NULL," \n",&saveptr);     // get name
                s2 = strtok_r(NULL," \n",&saveptr);     // get  value

                sprintf(sqlCmd,"update io_point set state ='%s' where name='%s';",s2, s1);
                strcpy(outBuffer, sqlCmd);
                printf("%s\n", outBuffer);

                int rc =mysql_query(conn,sqlCmd);
                if ( rc != 0 ) {
                    finish_with_error(conn);
                } else {
                    sprintf(outBuffer,"%s\n",s2);
                }
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
    printf("\tspreadToMysql takes messages from spread and sends them\n");
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
    printf("\t-P\t\tPrint config and exit.\n");

    printf("\n");
    printf("The defaults are:\n");
    printf("\tspreadToMysql -u user -s 4803@localhost \n");

    printf("\n");
}

void print_config() {
    printf("Current Configuration:\n");
    printf("  Database:\n");
    printf("    Host: %s\n", g.database.name ? g.database.name : "(not set)");
    printf("    Database: %s\n", g.database.db ? g.database.db : "(not set)");
    printf("    User: %s\n", g.database.user ? g.database.user : "(not set)");
    printf("    Password: %s\n", g.database.passwd ? "(set)" : "(not set)"); // Avoid printing password
    printf("  Spread:\n");
    printf("    Name: %s\n", g.spread.name ? g.spread.name : "(not set)");
    printf("    Port: %s\n", g.spread.port ? g.spread.port : "(not set)");
    printf("    Default Group: %s\n", g.spread.default_group ? g.spread.default_group : "(not set)");
    printf("    User: %s\n", g.spread.user ? g.spread.user : "(not set)");
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
    char *configFile = "/etc/mqtt/bridge.json";

    g.connectedToMysql = false;

//    char            user[32];
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
    char *user = NULL;

    mailbox         Mbox;

    group[0] = 0;

//    strcpy(user,"user");
    strcpy(server,"4803@localhost");
//    group[0] = 0;

    while ((ch = getopt(argc, argv, "c:f:lh?u:g:s:t:pvxP")) != -1) {

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
//                strcpy(g.spread.default_group, optarg);
                break;
            case 'h':
            case '?':
                usage();
                exit(0);
                break;
            case 'P':
                if (configFile) {
                    if (load_config(configFile, &g) != 0) {
                        fprintf(stderr, "Failed to load config file: %s\n", configFile);
                        return 1;
                    }
                }
                print_config();

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
                user=strdup(optarg);
//                g.spread.user=strdup(optarg);
//                strcpy(g.spread.user, optarg);
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
    }

    if(user) {
        g.spread.user=user;
    }

    print_config();

    ret = SP_connect(server, g.spread.user, 0, 1, &Mbox, Private_group);

    if (ret < 0) {
        SP_error(ret);
        exit(1);
    }
    printf("I am %s\n", Private_group);
//    SP_join(Mbox, "global");
    SP_join(Mbox, g.spread.default_group);

    if (group[0] != 0) {
        SP_join(Mbox,group);
    }

    if (strlen(g.spread.default_group) > 0 ) {
        SP_join(Mbox, g.spread.default_group);
    }

    MYSQL *conn = mysql_init(NULL);

    if(mysql_real_connect(conn,g.database.name, g.database.user, g.database.passwd, g.database.db, 0,NULL,0) == NULL) {
        fprintf(stderr,"%sb",mysql_error(conn));
        finish_with_error(conn);
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

            char *t = Interp(message, conn);

            printf("Sender is %s\n", sender);
            if(strcmp(sender, Private_group)) {
                ret = SP_multicast(Mbox,AGREED_MESS, sender, 1, strlen(t),t);
            }

            if(loop ==0) {
                exitFlag = 1;
            } else {
                exitFlag=0;
            }
        } else if(Is_membership_mess(service_type)) {
            printf("%s\n",sender);
        }
    } while (!exitFlag);

    ret = SP_leave(Mbox, g.spread.default_group);

    exit(0);

}
