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

char *Interp(char *);

struct Global {
	bool connectedToMysql;
        char *database_name;    // The nmame or ip address of the MySQL instence
        char *db_name;          // The database instance.
        char *user_name;
        char *passwd;
};

struct Global g;

char *Interp(char *cmd) {
        char *c;
        char *saveptr;
        char *token;
        static char outBuffer[BUFFER_LEN];
        int result;

        (void *)memset(outBuffer,0,BUFFER_LEN);

	if(cmd[0] == '^') {

            token = strtok_r(cmd," \n", &saveptr);

            if (!strcmp(token,"^status")){
                result = sprintf(outBuffer,"\nConnected to mySql:%d\n",(int)g.connectedToMysql);
            } else if (!strcmp(token,"^ping")) {
                strcpy(outBuffer,"+pong");
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
// 
// Jansson library
//

    json_t *root;
    json_error_t error;

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
        root= json_load_file(configFile,0,&error);
        if(!root) {
            fprintf(stderr,"Error loading/opening file %s\n", configFile);
            return 1;
        }

        if(!json_is_object(root)) {     
            fprintf(stderr, "Error: Root element is not an object\n");
            return 1;
        }
    }
    json_t *database_object = json_object_get(root,"database");
    json_t *name_obj = NULL;
    json_t *name_value = NULL;

    if(json_is_object(database_object)) {
        name_obj = json_object_get(database_object,"name");
        if(json_is_string(name_obj)) {
            printf("Host name is %s\n", json_string_value(name_obj));
            g.database_name=strdup( json_string_value(name_obj));
        }
        json_t *db_obj = json_object_get(database_object,"db");
        if(json_is_string(db_obj)) {
            printf("db name is %s\n", json_string_value(db_obj));
            g.db_name=strdup( json_string_value(db_obj));
        }
        json_t *user_obj = json_object_get(database_object,"user");
        if(json_is_string(user_obj)) {
            printf("User name is %s\n", json_string_value(user_obj));
            g.user_name=strdup( json_string_value(user_obj));
        }
        json_t *passwd_obj = json_object_get(database_object,"passwd");
        if(json_is_string(passwd_obj)) {
            printf("Password is %s\n", json_string_value(passwd_obj));
            g.passwd=strdup( json_string_value(passwd_obj));
        }
    }


    if(verbose) {
        printf("=========\n");
        printf("MySQL Host  :%s\n",g.database_name);
        printf("MySQL db    :%s\n",g.db_name);
        printf("MySQL User  :%s\n",g.user_name);
        printf("MySQL Passwd:%s\n",g.passwd);
        printf("\n");
        printf("Spread Server:%s\n",server);
        printf("Spread Group :%s\n",group);
        printf("=========\n");
    }
    ret = SP_connect(server, user, 0, 1, &Mbox, Private_group);

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
