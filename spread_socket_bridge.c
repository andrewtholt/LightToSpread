#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

// Spread Toolkit primary header
#include <sp.h> // This is the main header for the C API now

// --- Configuration ---
#define SPREAD_DAEMON_IP       "127.0.0.1"
#define SPREAD_DAEMON_PORT     4803
#define SPREAD_PRIVATE_NAME    "c_socket_bridge"
#define SPREAD_GROUP           "global"
#define MESSAGE_TYPE           1 // Example Spread message type

#define SOCKET_HOST            "0.0.0.0"
#define SOCKET_PORT            12345
#define BUFFER_SIZE            4096
#define MAX_PENDING_CONNECTIONS 5

// --- Global Variables for Spread Connection ---
char           spread_private_group[MAX_GROUP_NAME];
char           spread_private_name_buf[MAX_GROUP_NAME];
mailbox        spread_mailbox;
int            spread_connected = 0;
int            verbose = 0; // Verbose flag

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

    if (verbose) {
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
    if (verbose) printf("Joined Spread group '%s'\n", spread_group);
    spread_connected = 1;
    return 0;
}

// --- Function to Disconnect from Spread ---
void disconnect_from_spread(char *spread_group) {
    if (spread_connected) {
        if (verbose) printf("Leaving Spread group '%s' and disconnecting...\n", spread_group);
        SP_leave(spread_mailbox, spread_group);
        SP_disconnect(spread_mailbox);
        spread_connected = 0;
    }
}

void print_usage(char *prog_name) {
    fprintf(stderr, "Usage: %s [-h] [-v] [-i <ip_address>] [-p <port>] [-g <group_name>] [-u <user_name>]\n", prog_name);
    fprintf(stderr, "    -h: Print this help message.\n");
    fprintf(stderr, "    -v: Verbose output.\n");
    fprintf(stderr, "    -i: Spread daemon IP address (default: %s).\n", SPREAD_DAEMON_IP);
    fprintf(stderr, "    -p: Spread daemon port (default: %d).\n", SPREAD_DAEMON_PORT);
    fprintf(stderr, "    -g: Spread group to join (default: %s).\n", SPREAD_GROUP);
    fprintf(stderr, "    -u: Spread user name (default: %s).\n", SPREAD_PRIVATE_NAME);
    exit(0);
}

int main(int argc, char *argv[]) {
    int listen_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[BUFFER_SIZE];
    int ret;
    int opt;
    char *spread_ip = SPREAD_DAEMON_IP;
    int spread_port = SPREAD_DAEMON_PORT;
    char *spread_group = SPREAD_GROUP;
    char *spread_user = SPREAD_PRIVATE_NAME;

    while ((opt = getopt(argc, argv, "hvi:p:g:u:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'i':
                spread_ip = optarg;
                break;
            case 'p':
                spread_port = atoi(optarg);
                break;
            case 'g':
                spread_group = optarg;
                break;
            case 'u':
                spread_user = optarg;
                break;
            default:
                print_usage(argv[0]);
                break;
        }
    }

    fd_set read_fds;
    int max_fd;

    if (verbose) printf("Spread Socket Bridge (C) starting...\n");

    // --- 1. Connect to Spread ---
    if (connect_to_spread(spread_ip, spread_port, spread_user, spread_group) != 0) {
        return 1; // Exit if Spread connection fails
    }


    // --- 2. Create Listening Network Socket ---
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        disconnect_from_spread(spread_group);
        return 1;
    }

    // Allow reusing address
    int optval = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        close(listen_sock);
        disconnect_from_spread(spread_group);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SOCKET_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SOCKET_HOST);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(listen_sock);
        disconnect_from_spread(spread_group);
        return 1;
    }

    if (listen(listen_sock, MAX_PENDING_CONNECTIONS) == -1) {
        perror("listen");
        close(listen_sock);
        disconnect_from_spread(spread_group);
        return 1;
    }

    if (verbose) printf("Listening for incoming network connections on %s:%d...\n", SOCKET_HOST, SOCKET_PORT);

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
                if (verbose) printf("Accepted connection from %s:%d\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                // For simplicity, we only handle one client at a time.
                // If another client connects, the previous one would be dropped if we don't fork/thread.
            }
        }

        // --- Handle Data from Network Client (if connected) ---
        if (client_sock != -1 && FD_ISSET(client_sock, &read_fds)) {
            ret = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (ret <= 0) {
                if (ret == 0) {
                    if (verbose) printf("Client %s:%d disconnected.\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                } else {
                    perror("recv");
                }
                close(client_sock);
                client_sock = -1; // Mark as no client connected
            } else {
                buffer[ret] = '\0'; // Null-terminate the received data
                if (verbose) printf("Received from socket (%d bytes): %s", ret, buffer); // Use %s directly, as it might have a newline

                // Forward to Spread
                ret = SP_multicast(spread_mailbox, AGREED_MESS, spread_group, MESSAGE_TYPE,
                                   strlen(buffer), buffer);
                if (ret < 0) {
                    SP_error(ret);
                    // Decide how to handle this error (e.g., attempt reconnect, log)
                } else {
                    if (verbose) printf("Forwarded to Spread group '%s'.\n", spread_group);
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
                             &mess_type, &endian_mismatch, BUFFER_SIZE, buffer);

            if (ret < 0) {
                SP_error(ret);
                if (Is_causal_mess(ret)) {
                    fprintf(stderr, "Spread daemon disconnected. Attempting to reconnect...\n");
                    disconnect_from_spread(spread_group);
                    if (connect_to_spread(spread_ip, spread_port, spread_user, spread_group) != 0) {
                        fprintf(stderr, "Reconnection to Spread failed. Exiting.\n");
                        break; // Exit if cannot reconnect
                    }
                }
                continue; // Continue loop after error
            }

            buffer[ret] = '\0'; // Null-terminate the received message

            // Process Spread message based on service_type
            if (Is_regular_mess(service_type)) {
                if (verbose) printf("Received from Spread (Sender: %s, Group: %s, Type: %d, Size: %d): %s\n",
                       sender, target_groups[0], mess_type, ret, buffer); // Using target_groups[0] for simplicity

                // Forward to Network Socket (if connected)
                if (client_sock != -1) {
                    char formatted_message[BUFFER_SIZE + 128]; // Increased size for formatting
                    snprintf(formatted_message, sizeof(formatted_message),
                             "[SPREAD from %s@%s]: %s\n", sender, target_groups[0], buffer);

                    int bytes_sent = send(client_sock, formatted_message, strlen(formatted_message), 0);
                    if (bytes_sent == -1) {
                        perror("send to socket");
                        // Client might have disconnected, close socket
                        close(client_sock);
                        client_sock = -1;
                    } else {
                        if (verbose) printf("Forwarded to socket (%d bytes).\n", bytes_sent);
                    }
                }
            } else if (Is_membership_mess(service_type)) {
                if (verbose) printf("Received Membership Message (not forwarded): Service Type: %d\n", service_type);
                // You might want to handle membership changes, but for this bridge, we ignore for now.
            } else {
                if (verbose) printf("Received unknown Spread message (Service Type: %d, Size: %d)\n", service_type, ret);
            }
        }
    }

    // --- Cleanup ---
    if (client_sock != -1) {
        close(client_sock);
    }
    close(listen_sock);
    disconnect_from_spread(spread_group);

    if (verbose) printf("Spread Socket Bridge (C) shutting down.\n");
    return 0;
}
