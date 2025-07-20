#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sp.h> // Spread Toolkit header

#define MAX_MESSAGE_LEN 1024
#define GROUP_NAME "my_io_group" // The Spread group to connect to

mailbox Mbox;
char private_group[MAX_GROUP_NAME];
char daemon_address[MAX_GROUP_NAME] = "127.0.0.1"; // Default Spread daemon address
int debug = 0; // Set to 1 for more verbose Spread debugging

// Thread function to receive messages from Spread
void *receive_thread_func(void *arg) {
    char sender[MAX_GROUP_NAME];
    // CORRECTED LINE: Using MAX_MEMBERS_IN_GROUP for target_groups array size
    char target_groups[MAX_MEMBERS_IN_GROUP][MAX_GROUP_NAME];
    int num_target_groups;
    short service_type;
    int endian_mismatch;
    int mess_type;
    int group_id;
    int num_vs_sets;
    vs_set_info vs_sets[MAX_VS_SETS];
    int ret;
    char mess[MAX_MESSAGE_LEN];
    int mess_len;

    while (1) {
        mess_len = MAX_MESSAGE_LEN;
        // CORRECTED LINE: Passing MAX_MEMBERS_IN_GROUP as the max_groups parameter
        ret = SP_receive(Mbox, &service_type, sender, MAX_MEMBERS_IN_GROUP, &num_target_groups, target_groups,
                         &mess_type, &endian_mismatch, sizeof(mess), mess, &mess_len);

        if (ret < 0) {
            if (ret == CONNECTION_CLOSED) {
                fprintf(stderr, "Spread daemon connection closed. Exiting receive thread.\n");
                break;
            } else {
                SP_error(ret);
                fprintf(stderr, "Error receiving message from Spread. Continuing...\n");
                usleep(100000); // Wait a bit before retrying
                continue;
            }
        }

        mess[mess_len] = '\0'; // Null-terminate the received message

        if (Is_regular_mess(service_type)) {
            // Regular message, print to stdout
            printf("%s\n", mess);
            fflush(stdout);
        } else if (Is_membership_mess(service_type)) {
            // Membership message (group changes, etc.), for debugging
            if (debug) {
                char memb_type[128];
                SP_get_memb_info(service_type, &group_id, &num_vs_sets, vs_sets, &ret);
                if (ret < 0) {
                    SP_error(ret);
                    continue;
                }
                SP_group_type_to_string(service_type, memb_type);
                fprintf(stderr, "Membership message: %s for group %s from %s\n", memb_type, sender, private_group);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // ... rest of the main function remains the same ...
    int ret;
    pthread_t recv_tid;
    char line[MAX_MESSAGE_LEN];

    if (argc > 1) {
        strncpy(daemon_address, argv[1], MAX_GROUP_NAME - 1);
        daemon_address[MAX_GROUP_NAME - 1] = '\0';
    }

    if (debug) {
        printf("Attempting to connect to Spread daemon at %s\n", daemon_address);
    }

    // Connect to the Spread daemon
    ret = SP_connect(daemon_address, "stdin_stdout_client", 0, 1, &Mbox, private_group);
    if (ret != SPREAD_OK) {
        SP_error(ret);
        fprintf(stderr, "Error connecting to Spread daemon.\n");
        exit(1);
    }
    printf("Connected to Spread daemon with private group %s\n", private_group);

    // Join the specified Spread group
    ret = SP_join(Mbox, GROUP_NAME);
    if (ret != SPREAD_OK) {
        SP_error(ret);
        SP_disconnect(Mbox);
        fprintf(stderr, "Error joining Spread group %s.\n", GROUP_NAME);
        exit(1);
    }
    printf("Joined Spread group %s\n", GROUP_NAME);

    // Create the receive thread
    if (pthread_create(&recv_tid, NULL, receive_thread_func, NULL) != 0) {
        perror("Failed to create receive thread");
        SP_disconnect(Mbox);
        exit(1);
    }

    // Main loop: read from stdin and send to Spread
    printf("Ready. Type messages to send to the Spread group. Press Ctrl+D to exit.\n");
    while (fgets(line, sizeof(line), stdin) != NULL) {
        // Remove trailing newline character
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) > 0) {
            ret = SP_multicast(Mbox, AGREED_MESS, GROUP_NAME, 0, strlen(line), line);
            if (ret < 0) {
                SP_error(ret);
                fprintf(stderr, "Error sending message to Spread. Continuing...\n");
            }
        }
    }

    printf("Exiting. Disconnecting from Spread.\n");

    // Cancel the receive thread (optional, but good practice for clean shutdown)
    pthread_cancel(recv_tid);
    pthread_join(recv_tid, NULL); // Wait for the thread to terminate

    // Disconnect from Spread
    ret = SP_disconnect(Mbox);
    if (ret != SPREAD_OK) {
        SP_error(ret);
        fprintf(stderr, "Error disconnecting from Spread.\n");
    }

    return 0;
}
