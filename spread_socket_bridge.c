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

// Spread Toolkit primary header
#include <sp.h> // This is the main header for the C API now

// --- Configuration Structure ---
typedef struct {
    char spread_daemon_ip[64];
    int spread_daemon_port;
    char spread_private_name[MAX_GROUP_NAME];
    char spread_group[MAX_GROUP_NAME];
    char socket_host[64];
    int socket_port;
    int my_port; // New config variable for the listening port
    int buffer_size;
    int max_pending_connections;
    int verbose;
} config_t;

// Default configuration values
#define DEFAULT_SPREAD_DAEMON_IP       "127.0.0.1"
#define DEFAULT_SPREAD_DAEMON_PORT     4803
#define DEFAULT_SPREAD_PRIVATE_NAME    "c_socket_bridge"
#define DEFAULT_SPREAD_GROUP           "global"
#define DEFAULT_SOCKET_HOST            "0.0.0.0"
#define DEFAULT_SOCKET_PORT            12345
#define DEFAULT_MY_PORT                12345 // Default for the new listening port
#define DEFAULT_BUFFER_SIZE            4096
#define DEFAULT_MAX_PENDING_CONNECTIONS 5

#define MESSAGE_TYPE           1 // Example Spread message type (fixed)


// --- Global Variables for Spread Connection ---
char           spread_private_group[MAX_GROUP_NAME];
char           spread_private_name_buf[MAX_GROUP_NAME];
mailbox        spread_mailbox;
int            spread_connected = 0;
config_t       app_config; // Global configuration instance

#define MAX_ADDITIONAL_GROUPS 10
char *additional_groups[MAX_ADDITIONAL_GROUPS];
int num_additional_groups = 0;

// --- Function to Print Configuration ---
void print_config(const config_t *config) {
    printf("\n--- Loaded Configuration ---\n");
    printf("  Spread Daemon IP: %s\n", config->spread_daemon_ip);
    printf("  Spread Daemon Port: %d\n", config->spread_daemon_port);
    printf("  Spread Private Name: %s\n", config->spread_private_name);
    printf("  Spread Group: %s\n", config->spread_group);
    printf("  Socket Host: %s\n", config->socket_host);
    printf("  Socket Port (for Spread messages): %d\n", config->socket_port);
    printf("  My Listening Port: %d\n", config->my_port);
    printf("  Buffer Size: %d\n", config->buffer_size);
    printf("  Max Pending Connections: %d\n", config->max_pending_connections);
    printf("  Verbose: %s\n", config->verbose ? "true" : "false");
    printf("----------------------------\n\n");
}

// --- Function to Load Configuration from JSON File ---
int load_config_from_json(const char *config_file_path, config_t *config) {
    json_t *root;
    json_error_t error;

    // Set default values
    strncpy(config->spread_daemon_ip, DEFAULT_SPREAD_DAEMON_IP, sizeof(config->spread_daemon_ip) - 1);
    config->spread_daemon_ip[sizeof(config->spread_daemon_ip) - 1] = '\0';
    config->spread_daemon_port = DEFAULT_SPREAD_DAEMON_PORT;
    strncpy(config->spread_private_name, DEFAULT_SPREAD_PRIVATE_NAME, sizeof(config->spread_private_name) - 1);
    config->spread_private_name[sizeof(config->spread_private_name) - 1] = '\0';
    strncpy(config->spread_group, DEFAULT_SPREAD_GROUP, sizeof(config->spread_group) - 1);
    config->spread_group[sizeof(config->spread_group) - 1] = '\0';
    strncpy(config->socket_host, DEFAULT_SOCKET_HOST, sizeof(config->socket_host) - 1);
    config->socket_host[sizeof(config->socket_host) - 1] = '\0';
    config->socket_port = DEFAULT_SOCKET_PORT;
    config->my_port = DEFAULT_MY_PORT; // Initialize new config variable
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

    json_t *value;

    if ((value = json_object_get(root, "spread_daemon_ip")) && json_is_string(value)) {
        strncpy(config->spread_daemon_ip, json_string_value(value), sizeof(config->spread_daemon_ip) - 1);
        config->spread_daemon_ip[sizeof(config->spread_daemon_ip) - 1] = '\0';
    }
    if ((value = json_object_get(root, "spread_daemon_port")) && json_is_integer(value)) {
        config->spread_daemon_port = json_integer_value(value);
    }
    if ((value = json_object_get(root, "spread_private_name")) && json_is_string(value)) {
        strncpy(config->spread_private_name, json_string_value(value), sizeof(config->spread_private_name) - 1);
        config->spread_private_name[sizeof(config->spread_private_name) - 1] = '\0';
    }
    if ((value = json_object_get(root, "spread_group")) && json_is_string(value)) {
        strncpy(config->spread_group, json_string_value(value), sizeof(config->spread_group) - 1);
        config->spread_group[sizeof(config->spread_group) - 1] = '\0';
    }
    if ((value = json_object_get(root, "socket_host")) && json_is_string(value)) {
        strncpy(config->socket_host, json_string_value(value), sizeof(config->socket_host) - 1);
        config->socket_host[sizeof(config->socket_host) - 1] = '\0';
    }
    if ((value = json_object_get(root, "socket_port")) && json_is_integer(value)) {
        config->socket_port = json_integer_value(value);
    }
    if ((value = json_object_get(root, "my_port")) && json_is_integer(value)) {
        config->my_port = json_integer_value(value);
    }
    if ((value = json_object_get(root, "buffer_size")) && json_is_integer(value)) {
        config->buffer_size = json_integer_value(value);
    }
    if ((value = json_object_get(root, "max_pending_connections")) && json_is_integer(value)) {
        config->max_pending_connections = json_integer_value(value);
    }
    if ((value = json_object_get(root, "verbose")) && json_is_boolean(value)) {
        config->verbose = json_is_true(value);
    }

    json_decref(root);
    return 0;
}

// --- Function to Connect to Spread ---
int connect_to_spread(char *spread_ip, int spread_port, char *spread_user, char *spread_group) {
    int ret;
    int group_membership = 0; // Not requesting group membership notifications
    int priority = 1;         // Example priority
    int settle_timeout = 0;   // No settlement timeout
    char spread_daemon_address[128];

    sprintf(spread_private_name_buf, "%s@%s", spread_user, spread_ip);
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
            if (app_config.verbose) printf("Joined additional Spread group '%s'\n", additional_groups[i]);
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

void print_usage(char *prog_name) {
    fprintf(stderr, "Usage: %s [-h] [-v] [-c <config_file_path>] [-g <group_name>]...\n", prog_name);
    fprintf(stderr, "    -h: Print this help message.\n");
    fprintf(stderr, "    -v: Enable verbose output (overrides config file setting).\n");
    fprintf(stderr, "    -c: Path to the JSON configuration file (default: /etc/mqtt/bridge.json).\n");
    fprintf(stderr, "    -g: Join an additional Spread group. Can be specified multiple times.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    int listen_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    char *buffer;
    int ret;
    int opt;
    const char *config_file = "/etc/mqtt/bridge.json";

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "hvc:g:")) != -1) {
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
            default:
                print_usage(argv[0]);
                break;
        }
    }



    // Load configuration from JSON file
    if (load_config_from_json(config_file, &app_config) != 0) {
        fprintf(stderr, "Failed to load configuration. Exiting.\n");
        return 1;
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

    if (app_config.verbose) printf("Spread Socket Bridge (C) starting...\n");

    // --- 1. Connect to Spread ---
    if (connect_to_spread(app_config.spread_daemon_ip, app_config.spread_daemon_port, app_config.spread_private_name, app_config.spread_group) != 0) {
        free(buffer);
        return 1; // Exit if Spread connection fails
    }


    // --- 2. Create Listening Network Socket ---
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        disconnect_from_spread(app_config.spread_group);
        free(buffer);
        return 1;
    }

    // Allow reusing address
    int optval = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        close(listen_sock);
        disconnect_from_spread(app_config.spread_group);
        free(buffer);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(app_config.my_port);
    server_addr.sin_addr.s_addr = inet_addr(app_config.socket_host);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(listen_sock);
        disconnect_from_spread(app_config.spread_group);
        free(buffer);
        return 1;
    }

    if (listen(listen_sock, app_config.max_pending_connections) == -1) {
        perror("listen");
        close(listen_sock);
        disconnect_from_spread(app_config.spread_group);
        free(buffer);
        return 1;
    }

    if (app_config.verbose) printf("Listening for incoming network connections on %s:%d...\n", app_config.socket_host, app_config.my_port);

    client_sock = -1; // No client connected initially

    // --- Main Loop: Handle I/O with select() ---
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds); // Always monitor the listening socket

        max_fd = listen_sock;

        if (client_sock != -1) {
            FD_SET(client_sock, &read_fds); // Monitor connected client socket
            if (client_sock > max_fd) {
                max_fd = client_sock;
            }
        }

        // Monitor Spread mailbox
        FD_SET(spread_mailbox, &read_fds);
        if (spread_mailbox > max_fd) {
            max_fd = spread_mailbox;
        }

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
            // printf("Select timeout (no activity)...\n");
            continue; // Timeout, no data to read
        }

        // --- Handle Incoming Network Connection ---
        if (FD_ISSET(listen_sock, &read_fds)) {
            client_addr_len = sizeof(client_addr);
            client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_sock == -1) {
                perror("accept");
                // Continue listening for other connections
            } else {
                if (app_config.verbose) printf("Accepted connection from %s:%d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                // For simplicity, we only handle one client at a time.
                // If another client connects, the previous one would be dropped if we don't fork/thread.
            }
        }

        // --- Handle Data from Network Client (if connected) ---
        if (client_sock != -1 && FD_ISSET(client_sock, &read_fds)) {
            ret = recv(client_sock, buffer, app_config.buffer_size - 1, 0);
            if (ret <= 0) {
                if (ret == 0) {
                    if (app_config.verbose) printf("Client %s:%d disconnected.\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                } else {
                    perror("recv");
                }
                close(client_sock);
                client_sock = -1; // Mark as no client connected
            } else {
                buffer[ret] = '\0'; // Null-terminate the received data
                if (app_config.verbose) printf("Received from socket (%d bytes): %s", ret, buffer); // Use %s directly, as it might have a newline

                // Remove trailing newline or carriage return characters
                size_t len = strlen(buffer);
                while (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
                    buffer[--len] = '\0';
                }

                // Check for the "^quit" command
                if (strcmp(buffer, "^quit") == 0) {
                    if (app_config.verbose) printf("Received ^quit command. Shutting down.\n");
                    close(client_sock);
                    close(listen_sock);
                    disconnect_from_spread(app_config.spread_group);
                    free(buffer);
                    return 0; // Exit the program
                }

                // Forward to Spread
                ret = SP_multicast(spread_mailbox, AGREED_MESS, app_config.spread_group, MESSAGE_TYPE,
                                   strlen(buffer), buffer);
                if (ret < 0) {
                    SP_error(ret);
                    // Decide how to handle this error (e.g., attempt reconnect, log)
                } else {
                    if (app_config.verbose) printf("Forwarded to Spread group '%s'.\n", app_config.spread_group);
                }
            }
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
                    disconnect_from_spread(app_config.spread_group);
                    if (connect_to_spread(app_config.spread_daemon_ip, app_config.spread_daemon_port, app_config.spread_private_name, app_config.spread_group) != 0) {
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

                // Forward to Network Socket (if connected)
                if (client_sock != -1) {
                    char formatted_message[app_config.buffer_size + 128]; // Increased size for formatting
                    snprintf(formatted_message, sizeof(formatted_message),
                             "[SPREAD from %s@%s]: %s\n", sender, target_groups[0], buffer);

                    int bytes_sent = send(client_sock, formatted_message, strlen(formatted_message), 0);
                    if (bytes_sent == -1) {
                        perror("send to socket");
                        // Client might have disconnected, close socket
                        close(client_sock);
                        client_sock = -1;
                    } else {
                        if (app_config.verbose) printf("Forwarded to socket (%d bytes).\n", bytes_sent);
                    }
                }
            } else if (Is_membership_mess(service_type)) {
                if (app_config.verbose) printf("Received Membership Message (not forwarded): Service Type: %d\n", service_type);
                // You might want to handle membership changes, but for this bridge, we ignore for now.
            } else {
                if (app_config.verbose) printf("Received unknown Spread message (Service Type: %d, Size: %d)\n", service_type, ret);
            }
        }
    }

    // --- Cleanup ---
    if (client_sock != -1) {
        close(client_sock);
    }
    close(listen_sock);
    disconnect_from_spread(app_config.spread_group);
    free(buffer);

    if (app_config.verbose) printf("Spread Socket Bridge (C) shutting down.\n");
    return 0;
}
