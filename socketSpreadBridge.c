#include "sp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jansson.h>
#include <getopt.h>
#include <fcntl.h>

#define MAX_MESSLEN 102400
#define MAX_VSSETS      10
#define MAX_MEMBERS     100

// Configuration Structs
typedef struct {
    char *name;
    char *port;
    char *default_group;
    char *user;
} SpreadConfig;

typedef struct {
    char *name;
    char *port;
} SocketConfig;

struct Global {
    SpreadConfig spread;
    SocketConfig sock;
};

struct Global g;

// Prototypes
void usage(const char *prog_name);
void print_config();
int load_config(const char *filename, struct Global *g);
void sig_handler(int signo);

// Helper to extract string from JSON
void extract_string(json_t *parent, const char *key, char **target) {
    json_t *obj = json_object_get(parent, key);
    if (json_is_string(obj)) {
        if (*target) free(*target);
        *target = strdup(json_string_value(obj));
    }
}

int load_config(const char *filename, struct Global *g) {
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root) {
        fprintf(stderr, "error loading config: on line %d: %s\n", error.line, error.text);
        return 1;
    }

    json_t *spread_obj = json_object_get(root, "spread");
    if (spread_obj) {
        extract_string(spread_obj, "name", &g->spread.name);
        extract_string(spread_obj, "port", &g->spread.port);
        extract_string(spread_obj, "default_group", &g->spread.default_group);
        extract_string(spread_obj, "user", &g->spread.user);
    }

    json_t *socket_obj = json_object_get(root, "socket");
    if (socket_obj) {
        extract_string(socket_obj, "name", &g->sock.name);
        extract_string(socket_obj, "port", &g->sock.port);
    }

    json_decref(root);
    return 0;
}

void usage(const char *prog_name) {
    printf("\nUsage: %s [options]\n", prog_name);
    printf("\nDescription:\n");
    printf("\tBridges a TCP socket to a Spread group.\n");
    printf("\nOptions:\n");
    printf("\t-h, --help\t\tThis help message.\n");
    printf("\t-c, --config <file>\tPath to JSON config file. (Default: spreadToSocket.json)\n");
    printf("\t-u, --user <user>\tSpread user name.\n");
    printf("\t-g, --group <group>\tSpread group to join.\n");
    printf("\t-s, --server <server>\tSpread server address (e.g., 4803@localhost).\n");
    printf("\t-p, --port <port>\tTCP port to listen on.\n");
    printf("\t-P, --print-config\tPrint loaded configuration and exit.\n");
    printf("\t-r, --random\t\tRandom user name.\n");
    printf("\n");
}

void print_config() {
    printf("Current Configuration:\n");
    printf("  Spread:\n");
    printf("    Server: %s@%s\n", g.spread.port ? g.spread.port : "(not set)", g.spread.name ? g.spread.name : "(not set)");
    printf("    User: %s\n", g.spread.user ? g.spread.user : "(not set)");
    printf("    Group: %s\n", g.spread.default_group ? g.spread.default_group : "(not set)");
    printf("  Socket:\n");
    printf("    Listen IP: %s\n", g.sock.name ? g.sock.name : "0.0.0.0");
    printf("    Listen Port: %s\n", g.sock.port ? g.sock.port : "(not set)");
    printf("\n");
}

void sig_handler(int signo) {
    fprintf(stderr, "Received signal %d, exiting.\n", signo);
    exit(signo);
}

int main(int argc, char *argv[]) {
    // Default values
    g.spread.name = strdup("localhost");
    g.spread.port = strdup("4803");
    g.spread.user = strdup("spread_user");
    g.spread.default_group = strdup("spread_group");
    g.sock.name = strdup("127.0.0.1");
    g.sock.port = strdup("9191");
//    char *configFile = "spreadToSocket.json";
    char *configFile = "/etc/mqtt/bridge.json";
    char spread_server_str[255];

    // Command line parsing
    int ch;
    const char* short_opts = "hc:u:g:s:p:Pr";
    const struct option long_opts[] = {
        {"help", no_argument, NULL, 'h'},
        {"config", required_argument, NULL, 'c'},
        {"user", required_argument, NULL, 'u'},
        {"group", required_argument, NULL, 'g'},
        {"server", required_argument, NULL, 's'},
        {"port", required_argument, NULL, 'p'},
        {"print-config", no_argument, NULL, 'P'},
        {"random", no_argument, NULL, 'r'},
        {NULL, 0, NULL, 0}
    };

    // Load config file first, so command line args can override
    if (load_config(configFile, &g) != 0) {
        fprintf(stderr, "Could not load default config file: %s\n", configFile);
    }

    while ((ch = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (ch) {
            case 'c':
                configFile = optarg;
                if (load_config(configFile, &g) != 0) {
                   fprintf(stderr, "Failed to load config file: %s\n", configFile);
                   return 1;
                }
                break;
            case 'u':
                if (g.spread.user) free(g.spread.user);
                g.spread.user = strdup(optarg);
                break;
            case 'g':
                if (g.spread.default_group) free(g.spread.default_group);
                g.spread.default_group = strdup(optarg);
                break;
            case 's':
                sscanf(optarg, "%[^@ ]@%s", g.spread.port, g.spread.name);
                break;
            case 'p':
                if (g.sock.port) free(g.sock.port);
                g.sock.port = strdup(optarg);
                break;
            case 'P':
                print_config();
                exit(0);
            case 'h':
                usage(argv[0]);
                exit(0);
            case 'r':
                printf("Random user\n");
                {
                    if (g.spread.user) {
                        free(g.spread.user);
                    }
                    pid_t pid = getpid();
                    char *ptr;
                    char tmp[50];

                    sprintf(tmp,"BR%010d", pid);
                    g.spread.user = strdup(tmp);

                }
                break;
            default:
                usage(argv[0]);
                exit(0);
        }
    }
    
    print_config();
    snprintf(spread_server_str, sizeof(spread_server_str), "%s@%s", g.spread.port, g.spread.name);

    // Setup Spread connection
    mailbox Mbox;
    char Private_group[MAX_GROUP_NAME];
    int ret = SP_connect(spread_server_str, g.spread.user, 0, 1, &Mbox, Private_group);
    if (ret < 0) {
        SP_error(ret);
        exit(1);
    }
    printf("Connected to Spread as %s\n", Private_group);
    SP_join(Mbox, g.spread.default_group);
    printf("Joined Spread group %s\n", g.spread.default_group);

    // Setup listening socket
    int listen_fd, client_fd = -1;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    printf("Address %s\n",g.sock.name);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(g.sock.name);
    serv_addr.sin_port = htons(atoi(g.sock.port));

    if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }
    printf("Listening on %s:%s\n", g.sock.name, g.sock.port);

    // Main loop
    fd_set read_fds;
    int max_fd;
    static char message[MAX_MESSLEN];

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(Mbox, &read_fds);
        FD_SET(listen_fd, &read_fds);
        max_fd = (Mbox > listen_fd) ? Mbox : listen_fd;

        if (client_fd != -1) {
            FD_SET(client_fd, &read_fds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select");
            continue;
        }

        // Check for new socket connection
        if (FD_ISSET(listen_fd, &read_fds)) {
            int new_client = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (new_client < 0) {
                perror("accept");
            } else {
                if (client_fd != -1) {
                    printf("Replacing existing client connection.\n");
                    close(client_fd);
                }
                client_fd = new_client;
                printf("Accepted new connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
        }

        // Check for data from Spread
        if (FD_ISSET(Mbox, &read_fds)) {
            static char sender[MAX_GROUP_NAME];
            static char target_groups[100][MAX_GROUP_NAME];
            int num_groups;
            int16 mess_type;
            int endian_mismatch;
            int service_type;

            ret = SP_receive(Mbox, &service_type, sender, 100, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(message), message);
            if (ret < 0) {
                SP_error(ret);
                exit(1);
            }

            if (Is_regular_mess(service_type)) {
                if (client_fd != -1) {
                    message[ret] = 0;
                    printf("Spread -> Socket: %s\n", message);
                    if (write(client_fd, message, ret) < 0) {
                        perror("write to client");
                        close(client_fd);
                        client_fd = -1;
                    }
                }
            }
        }

        // Check for data from socket
        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds)) {
            ret = read(client_fd, message, sizeof(message) - 1);
            if (ret > 0) {
                message[ret] = 0;
                printf("Socket -> Spread: %s\n", message);
                ret = SP_multicast(Mbox, AGREED_MESS, g.spread.default_group, 1, ret, message);
                if (ret < 0) {
                    SP_error(ret);
                    exit(1);
                }
            } else {
                if (ret == 0) printf("Client disconnected.\n");
                else perror("read from client");
                close(client_fd);
                client_fd = -1;
            }
        }
    }

    // Cleanup
    if (client_fd != -1) close(client_fd);
    close(listen_fd);
    SP_disconnect(Mbox);

    free(g.spread.name);
    free(g.spread.port);
    free(g.spread.user);
    free(g.spread.default_group);
    free(g.sock.name);
    free(g.sock.port);

    return 0;
}
