#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <jansson.h> // For JSON parsing
#include <mysql/mysql.h> // For MySQL connection

// Spread Toolkit primary header
#include <sp.h> // This is the main header for the C API now

#include "config.h"

// Default configuration values
#define DEFAULT_BUFFER_SIZE 4096
#define DEFAULT_MAX_PENDING_CONNECTIONS 5

#define MESSAGE_TYPE           1 // Example Spread message type (fixed)
#define MYSQL_QUERY_PREFIX     "MYSQL_QUERY:"

// --- Global Variables for Spread Connection ---
char           spread_private_group[MAX_GROUP_NAME];
char           spread_private_name_buf[MAX_GROUP_NAME];
mailbox        spread_mailbox;
int            spread_connected = 0;
BridgeConfig   app_config; // Global configuration instance

// --- Global Variable for MySQL Connection ---
MYSQL *conn;

#define MAX_ADDITIONAL_GROUPS 10
char *additional_groups[MAX_ADDITIONAL_GROUPS];
int num_additional_groups = 0;

// --- Function to Print Configuration ---
void print_config(const BridgeConfig *config) {
    printf("\n--- Loaded Configuration ---\n");
    printf("remote:\n");
    printf("  name: %s\n", config->remote.name);
    printf("  port: %s\n", config->remote.port);
    printf("  prefix: %s\n", config->remote.prefix);

    printf("local:\n");
    printf("  name: %s\n", config->local.name);
    printf("  port: %s\n", config->local.port);

    printf("common:\n");
    printf("  topic: %s\n", config->common.topic);

    printf("database:\n");
    printf("  name: %s\n", config->database.name);
    printf("  db: %s\n", config->database.db);
    printf("  user: %s\n", config->database.user);
    printf("  passwd: %s\n", config->database.passwd);

    printf("socket:\n");
    printf("  name: %s\n", config->socket.name);
    printf("  port: %s\n", config->socket.port);
    printf("  dispatch: %s\n", config->socket.dispatch);

    printf("topics:\n");
    printf("  sunrise: %s\n", config->topics.sunrise);
    printf("  sunset: %s\n", config->topics.sunset);
    printf("  time: %s\n", config->topics.time);
    printf("  day: %s\n", config->topics.day);
    printf("  dow: %s\n", config->topics.dow);

    printf("location:\n");
    printf("  long: %s\n", config->location.lon);
    printf("  lat: %s\n", config->location.lat);

    printf("home_assistant:\n");
    printf("  name: %s\n", config->home_assistant.name);
    printf("  port: %s\n", config->home_assistant.port);

    printf("spread:\n");
    printf("  name: %s\n", config->spread.name);
    printf("  port: %s\n", config->spread.port);
    printf("  user: %s\n", config->spread.user);
    printf("  default_group: %s\n", config->spread.default_group);

    printf("verbose: %s\n", config->verbose ? "true" : "false");
    printf("----------------------------\n\n");
}

// --- Function to Load Configuration from JSON File ---
int load_config_from_json(const char *config_file_path, BridgeConfig *config) {
    json_t *root;
    json_error_t error;

    // Set default values
    config->buffer_size = DEFAULT_BUFFER_SIZE;
    config->max_pending_connections = DEFAULT_MAX_PENDING_CONNECTIONS;
    config->verbose = 0; // Default to non-verbose

    root = json_load_file(config_file_path, 0, &error);

    if (!root) {
        fprintf(stderr, "Error loading config from %s: on line %d: %s\n",
                config_file_path, error.line, error.text);
        return -1;
    }

    if (!json_is_object(root)) {
        fprintf(stderr, "Error: Root of config file is not an object.\n");
        json_decref(root);
        return -1;
    }

    // Extract values from the JSON object
    json_t *remote_obj = json_object_get(root, "remote");
    if (remote_obj) {
        extract_string(remote_obj, "name", &config->remote.name);
        extract_string(remote_obj, "port", &config->remote.port);
        extract_string(remote_obj, "prefix", &config->remote.prefix);
    }

    json_t *local_obj = json_object_get(root, "local");
    if (local_obj) {
        extract_string(local_obj, "name", &config->local.name);
        extract_string(local_obj, "port", &config->local.port);
    }

    json_t *common_obj = json_object_get(root, "common");
    if (common_obj) {
        extract_string(common_obj, "topic", &config->common.topic);
    }

    json_t *database_obj = json_object_get(root, "database");
    if (database_obj) {
        extract_string(database_obj, "name", &config->database.name);
        extract_string(database_obj, "db", &config->database.db);
        extract_string(database_obj, "user", &config->database.user);
        extract_string(database_obj, "passwd", &config->database.passwd);
    }

    json_t *socket_obj = json_object_get(root, "socket");
    if (socket_obj) {
        extract_string(socket_obj, "name", &config->socket.name);
        extract_string(socket_obj, "port", &config->socket.port);
        extract_string(socket_obj, "dispatch", &config->socket.dispatch);
    }

    json_t *topics_obj = json_object_get(root, "topics");
    if (topics_obj) {
        extract_string(topics_obj, "sunrise", &config->topics.sunrise);
        extract_string(topics_obj, "sunset", &config->topics.sunset);
        extract_string(topics_obj, "time", &config->topics.time);
        extract_string(topics_obj, "day", &config->topics.day);
        extract_string(topics_obj, "dow", &config->topics.dow);
    }

    json_t *location_obj = json_object_get(root, "location");
    if (location_obj) {
        extract_string(location_obj, "long", &config->location.lon);
        extract_string(location_obj, "lat", &config->location.lat);
    }

    json_t *home_assistant_obj = json_object_get(root, "home_assistant");
    if (home_assistant_obj) {
        extract_string(home_assistant_obj, "name", &config->home_assistant.name);
        extract_string(home_assistant_obj, "port", &config->home_assistant.port);
    }

    json_t *spread_obj = json_object_get(root, "spread");
    if (spread_obj) {
        extract_string(spread_obj, "name", &config->spread.name);
        extract_string(spread_obj, "port", &config->spread.port);
        extract_string(spread_obj, "user", &config->spread.user);
        extract_string(spread_obj, "default_group", &config->spread.default_group);
    }

    json_t *value;
    if ((value = json_object_get(root, "verbose")) && json_is_boolean(value)) {
        config->verbose = json_is_true(value);
    }

    json_decref(root);
    return 0;
}

// --- Function to Connect to Spread ---
int connect_to_spread(char *spread_ip, int spread_port, char *spread_user, char *spread_group) {
    int ret;
    char spread_daemon_address[128];

    sprintf(spread_daemon_address, "%d@%s", spread_port, spread_ip);

    ret = SP_connect(spread_daemon_address, spread_user, 0, 1, &spread_mailbox, spread_private_group);

    if (ret < 0) {
        SP_error(ret);
        return -1;
    }

    if (app_config.verbose) {
        printf("Connected to Spread daemon on %s as %s. Mailbox: %d\n",
               spread_daemon_address, spread_private_name_buf, spread_mailbox);
    }

    // Join the specified Spread group
    ret = SP_join(spread_mailbox, spread_group);
    if (ret < 0) {
        SP_error(ret);
        SP_disconnect(spread_mailbox);
        return -1;
    }
    if (app_config.verbose) printf("Joined Spread group '%s'\n", spread_group);

    // Join additional Spread groups
    for (int i = 0; i < num_additional_groups; i++) {
        ret = SP_join(spread_mailbox, additional_groups[i]);
        if (ret < 0) {
            SP_error(ret);
            fprintf(stderr, "Warning: Failed to join additional Spread group '%s'.\n", additional_groups[i]);
        } else {
            if (app_config.verbose) printf("Joined additional Spread group '%s'.\n", additional_groups[i]);
        }
    }
    spread_connected = 1;
    return 0;
}

// --- Function to Disconnect from Spread ---
void disconnect_from_spread(char *spread_group) {
    if (spread_connected) {
        if (app_config.verbose) printf("Leaving Spread group '%s' and disconnecting...\n", spread_group);
        SP_leave(spread_mailbox, spread_group);

        // Leave additional Spread groups
        for (int i = 0; i < num_additional_groups; i++) {
            if (app_config.verbose) printf("Leaving additional Spread group '%s'.\n", additional_groups[i]);
            SP_leave(spread_mailbox, additional_groups[i]);
        }

        SP_disconnect(spread_mailbox);
        spread_connected = 0;
    }
}

// --- Function to Connect to MySQL ---
int connect_to_mysql(const char *host, const char *user, const char *password, const char *db_name) {
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return -1;
    }

    if (mysql_real_connect(conn, host, user, password, db_name, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        conn = NULL;
        return -1;
    }
    printf("Connected to MySQL database: %s\n", db_name);
    return 0;
}

// --- Function to Disconnect from MySQL ---
void disconnect_from_mysql() {
    if (conn) {
        mysql_close(conn);
        conn = NULL;
        printf("Disconnected from MySQL.\n");
    }
}

void interp(const char *sender, const char *query) {
    char response_buffer[app_config.buffer_size];
    int ret;

    char *s1 =NULL;
    char *s2 =NULL;
    char *s3 =NULL;
    char *save_ptr=NULL;
    char *buffer=NULL;
    char sqlCmd[DEFAULT_BUFFER_SIZE];

    // Allocate buffer based on config
    buffer = (char *)calloc(app_config.buffer_size,sizeof(unsigned char));
    if (buffer == NULL) {
        perror("malloc buffer");
        exit( 1);
    }
    strncpy(buffer, query, app_config.buffer_size);

    s1 = strtok_r(buffer," \r\n",&save_ptr);
    s2 = strtok_r(NULL," \r\n",&save_ptr);
    s3 = strtok_r(NULL," \r\n",&save_ptr);

    if (app_config.verbose) {
        printf("Executing command: %s\n", s1);
        printf("\t\t%s\n", s1);
    }

    if ( NULL == s1) {
        return;
    }

    if(!strncmp(s1,"GET",3)) {
        printf("GET\n");
        snprintf(sqlCmd, sizeof(sqlCmd), "select state from io_point where name = '%s';", s2);
        printf(sqlCmd);
//        MYSQL_RES *result = mysql_store_result(conn);
        if (mysql_query(conn, sqlCmd)) {
            fprintf(stderr,"mysql_query() failed.\n");
            mysql_close(conn);
            exit(1);
        }
        MYSQL_RES *res = mysql_store_result(conn);
        if (res == NULL) {
            fprintf(stderr,"mysql_store_result() failed.\n");
            mysql_close(conn);
            exit(1);
        }
        MYSQL_ROW row;
        int num=0;

        if((row = mysql_fetch_row(res)) != NULL) {
            printf("\n");
            printf("data %s \n",row[0]);
            snprintf(response_buffer, app_config.buffer_size,"%s\n",row[0] );
        }
    } else if (!strncmp(s1,"SET",3) ) {
        snprintf(sqlCmd, sizeof(sqlCmd), "update io_point set state='%s' where name = '%s';", s3,s2);
        printf("%s\n", sqlCmd);
        if (mysql_query(conn, sqlCmd)) {
            fprintf(stderr,"mysql_query() failed.\n");
            mysql_close(conn);
            exit(1);
        } else {
            sprintf(response_buffer,"%s\n", s3);
        }
    } else if (!strncmp(s1,"PING",4) ) {
        sprintf(response_buffer,"PONG\n");

    } else if (!strncmp(s1,"JGET",4) ) {
        sprintf(response_buffer,"RX: JGET\n");
        memset(sqlCmd,0,sizeof(sqlCmd));

//        sprintf(sqlCmd, "select JSON_OBJECT('type',io_type,'state',state ) as io_point from io_point where name='%s';", s2);
        sprintf(sqlCmd, "select JSON_OBJECT("
                    "'topic',topic,"
                    "'status_topic',status_topic,"
                    "'direction',direction,"
                    "'data_type',data_type,"
                    "'io_type',io_type"
                ") as mqtt_point from mqttQuery where name='%s' and enabled='YES';",s2);

        printf("%s\n",sqlCmd);
        if (mysql_query(conn, sqlCmd)) {
            fprintf(stderr,"mysql_query() failed.\n");
            mysql_close(conn);
            exit(1);
        }
        MYSQL_RES *res = mysql_store_result(conn);
        if (res == NULL) {
            fprintf(stderr,"mysql_store_result() failed.\n");
            mysql_close(conn);
            exit(1);
        }
        MYSQL_ROW row;
        int num=0;

        if((row = mysql_fetch_row(res)) != NULL) {
            printf("\n");
            printf("data %s \n",row[0]);
            snprintf(response_buffer, app_config.buffer_size,"%s\n",row[0] );
        }



    }
    ret = SP_multicast(spread_mailbox, AGREED_MESS, (char *)sender, MESSAGE_TYPE, strlen(response_buffer), response_buffer);
    if (ret < 0) {
        SP_error(ret);
        fprintf(stderr, "Failed to send MySQL response to Spread sender %s.\n", sender);
    } else {
        if (app_config.verbose) printf("Sent MySQL response to Spread sender %s.\n", sender);
    }
}

void print_usage(char *prog_name) {
    fprintf(stderr, "Usage: %s [-h] [-v] [-c <config_file_path>] [-g <group_name>] [-u <user_name>] [-D]...\n", prog_name);
    fprintf(stderr, "    -h: Print this help message.\n");
    fprintf(stderr, "    -v: Enable verbose output (overrides config file setting).\n");
    fprintf(stderr, "    -c: Path to the JSON configuration file (default: /etc/mqtt/bridge.json).\n");
    fprintf(stderr, "    -g: Join an additional Spread group. Can be specified multiple times.\n");
    fprintf(stderr, "    -u: Override the Spread user from the config file.\n");
    fprintf(stderr, "    -D: Dump the JSON config file to stdout and exit.\n");
    fprintf(stderr, "    -r: select a random user name.\n");
    exit(0);
}

void dump_json_config(const char *config_file_path) {
    FILE *fp = fopen(config_file_path, "r");
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        fwrite(buffer, 1, n, stdout);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    char *buffer;
    int ret;
    int opt;
    const char *config_file = "/etc/mqtt/bridge.json"; // Default config file
    char *user_override = NULL;
    char randomUser[32];
    int dump_config = 0;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "hvc:g:u:Dr")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                break;
            case 'v':
                app_config.verbose = 1; // Override verbose setting from config
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'g':
                if (num_additional_groups < MAX_ADDITIONAL_GROUPS) {
                    additional_groups[num_additional_groups++] = optarg;
                } else {
                    fprintf(stderr, "Warning: Maximum number of additional groups reached. Ignoring %s\n", optarg);
                }
                break;
            case 'u':
                user_override = optarg;
                break;
            case 'r':
                {
                    pid_t iam;
                    iam = getpid();
                    snprintf(randomUser,sizeof(randomUser),"mysql_%d", iam);
                    user_override = randomUser;
                }
                break;
            case 'D':
                dump_config = 1;
                break;
            default:
                print_usage(argv[0]);
                break;
        }
    }

    printf("Config file: %s\n", config_file);
    if (dump_config) {
        printf("Config file: %s\n", config_file);
        dump_json_config(config_file);
        return 0;
    }

    // Load configuration from JSON file
    if (load_config_from_json(config_file, &app_config) != 0) {
        fprintf(stderr, "Failed to load configuration. Exiting.\n");
        return 1;
    }

    // Override user if provided
    if (user_override) {
        app_config.spread.user = user_override;
    }

    if (app_config.verbose) {
        print_config(&app_config);
    }

    // Allocate buffer based on config
    buffer = (char *)malloc(app_config.buffer_size);
    if (buffer == NULL) {
        perror("malloc buffer");
        return 1;
    }

    fd_set read_fds;
    int max_fd;

    dump_json_config(config_file);

    if (app_config.verbose) printf("Spread to MySQL Bridge starting...\n");

    // --- 1. Connect to Spread ---
    if (connect_to_spread(app_config.spread.name, atoi(app_config.spread.port), app_config.spread.user, app_config.spread.default_group) != 0) {
        free(buffer);
        return 1; // Exit if Spread connection fails
    }

    // --- 2. Connect to MySQL ---
    if (connect_to_mysql(app_config.database.name, app_config.database.user, app_config.database.passwd, app_config.database.db) != 0) {
        disconnect_from_spread(app_config.spread.default_group);
        free(buffer);
        return 1; // Exit if MySQL connection fails
    }

    // --- Main Loop: Handle I/O with select() ---
    while (1) {
        FD_ZERO(&read_fds);
        // Monitor Spread mailbox
        FD_SET(spread_mailbox, &read_fds);
        max_fd = spread_mailbox;

        struct timeval timeout;
        timeout.tv_sec = 1;  // 1-second timeout
        timeout.tv_usec = 0;

        ret = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret == -1) {
            if (errno == EINTR) continue; // Interrupted by signal, just retry
            perror("select");
            break; // Exit on severe select error
        }

        if (ret == 0) {
            // printf("Select timeout (no activity)...");
            continue; // Timeout, no data to read
        }

        // --- Handle Messages from Spread ---
        if (FD_ISSET(spread_mailbox, &read_fds)) {
            char sender[MAX_GROUP_NAME];
            int num_groups;
            char target_groups[100][MAX_GROUP_NAME];
            int service_type;
            int16_t mess_type;
            int endian_mismatch;

            ret = SP_receive(spread_mailbox, &service_type, sender, MAX_GROUP_NAME,
                             &num_groups, target_groups,
                             &mess_type, &endian_mismatch, app_config.buffer_size, buffer);

            if (ret < 0) {
                SP_error(ret);
                if (Is_causal_mess(ret)) {
                    fprintf(stderr, "Spread daemon disconnected. Attempting to reconnect...\n");
                    disconnect_from_spread(app_config.spread.default_group);
                    if (connect_to_spread(app_config.spread.name, atoi(app_config.spread.port), app_config.spread.user, app_config.spread.default_group) != 0) {
                        fprintf(stderr, "Reconnection to Spread failed. Exiting.\n");
                        break; // Exit if cannot reconnect
                    }
                }
                continue; // Continue loop after error
            }

            buffer[ret] = '\0'; // Null-terminate the received message

            // Process Spread message based on service_type
            if (Is_regular_mess(service_type)) {
                if (app_config.verbose) printf("Received from Spread (Sender: %s, Group: %s, Type: %d, Size: %d): %s\n",
                       sender, target_groups[0], mess_type, ret, buffer); // Using target_groups[0] for simplicity

                char query[255];
                char *s1 = NULL;
                char *s2 = NULL;
                char *s3 = NULL;
                char *save_ptr=NULL;

                interp(sender,buffer);
                // Check if the message is a MySQL query
                // Check if this is a valid command, starting with a ^
                //
                /*
                if(!strncmp(buffer,"GET ",4)) {
                    s1 = strtok_r(buffer," ",&save_ptr);
                    s2 = strtok_r(NULL," \n",&save_ptr);
                    snprintf(query, sizeof(query), "select state from io_point where name = '%s';", s2);
                    // select state from io_point where name = '%s'
//                if (strstr(buffer, MYSQL_QUERY_PREFIX) == buffer) {
//                    const char *query = buffer + strlen(MYSQL_QUERY_PREFIX);
//                    execute_mysql_query_and_send_response(sender, query);
                } else if (!strncmp(buffer,"SET ",4)) {
                    s1 = strtok_r(buffer," ",&save_ptr);
                    s2 = strtok_r(NULL," \n",&save_ptr);
                    s3 = strtok_r(NULL," \n",&save_ptr);
                    snprintf(query,sizeof(query),"update io_point set state='%s' where name='%s';",s3, s2);
                    execute_mysql_query_and_send_response(sender, query);
                } else {
                    // Original logic: Insert into spread_messages table
                    char *escaped_message = (char *)malloc(2 * strlen(buffer) + 1); // Max 2x size + null terminator
                    if (escaped_message == NULL) {
                        perror("malloc escaped_message");
                        continue;
                    }
                    mysql_real_escape_string(conn, escaped_message, buffer, strlen(buffer));
                }
                */
            } else if (Is_membership_mess(service_type)) {
                if (app_config.verbose) printf("Received Membership Message (not forwarded): Service Type: %d\n", service_type);
            } else {
                if (app_config.verbose) printf("Received unknown Spread message (Service Type: %d, Size: %d)\n", service_type, ret);
            }
        }
    }

    // --- Cleanup ---
    disconnect_from_mysql();
    disconnect_from_spread(app_config.spread.default_group);
    free(buffer);

    if (app_config.verbose) printf("Spread to MySQL Bridge shutting down.\n");
    return 0;
}
