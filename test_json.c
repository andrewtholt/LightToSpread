#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

// Structs for the new JSON structure
typedef struct {
    char *name;
    char *port;
    char *prefix;
} RemoteConfig;

typedef struct {
    char *name;
    char *port;
} LocalConfig;

typedef struct {
    char *topic;
} CommonConfig;

typedef struct {
    char *name;
    char *db;
    char *user;
    char *passwd;
} DatabaseConfig;

typedef struct {
    char *name;
    char *port;
    char *dispatch;
} SocketConfig;

typedef struct {
    char *sunrise;
    char *sunset;
    char *time;
    char *day;
    char *dow;
} TopicsConfig;

typedef struct {
    char *lon;
    char *lat;
} LocationConfig;

typedef struct {
    char *name;
    char *port;
} HomeAssistantConfig;

typedef struct {
    RemoteConfig remote;
    LocalConfig local;
    CommonConfig common;
    DatabaseConfig database;
    SocketConfig socket;
    TopicsConfig topics;
    LocationConfig location;
    HomeAssistantConfig home_assistant;
} BridgeConfig;

// Function to print the configuration
void print_config(BridgeConfig *config) {
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
}

// Helper function to extract string value from a json object
void extract_string(json_t *parent, const char *key, char **target) {
    json_t *obj = json_object_get(parent, key);
    if (json_is_string(obj)) {
        *target = (char *)json_string_value(obj);
    }
}

int main() {
    json_t *root;
    json_error_t error;
    BridgeConfig config = {0};

    // Load the JSON file
    root = json_load_file("bridge_rpi3.json", 0, &error);
    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }

    // Extract values from the JSON object
    json_t *remote_obj = json_object_get(root, "remote");
    if (remote_obj) {
        extract_string(remote_obj, "name", &config.remote.name);
        extract_string(remote_obj, "port", &config.remote.port);
        extract_string(remote_obj, "prefix", &config.remote.prefix);
    }

    json_t *local_obj = json_object_get(root, "local");
    if (local_obj) {
        extract_string(local_obj, "name", &config.local.name);
        extract_string(local_obj, "port", &config.local.port);
    }

    json_t *common_obj = json_object_get(root, "common");
    if (common_obj) {
        extract_string(common_obj, "topic", &config.common.topic);
    }

    json_t *database_obj = json_object_get(root, "database");
    if (database_obj) {
        extract_string(database_obj, "name", &config.database.name);
        extract_string(database_obj, "db", &config.database.db);
        extract_string(database_obj, "user", &config.database.user);
        extract_string(database_obj, "passwd", &config.database.passwd);
    }

    json_t *socket_obj = json_object_get(root, "socket");
    if (socket_obj) {
        extract_string(socket_obj, "name", &config.socket.name);
        extract_string(socket_obj, "port", &config.socket.port);
        extract_string(socket_obj, "dispatch", &config.socket.dispatch);
    }

    json_t *topics_obj = json_object_get(root, "topics");
    if (topics_obj) {
        extract_string(topics_obj, "sunrise", &config.topics.sunrise);
        extract_string(topics_obj, "sunset", &config.topics.sunset);
        extract_string(topics_obj, "time", &config.topics.time);
        extract_string(topics_obj, "day", &config.topics.day);
        extract_string(topics_obj, "dow", &config.topics.dow);
    }

    json_t *location_obj = json_object_get(root, "location");
    if (location_obj) {
        extract_string(location_obj, "long", &config.location.lon);
        extract_string(location_obj, "lat", &config.location.lat);
    }

    json_t *home_assistant_obj = json_object_get(root, "home_assistant");
    if (home_assistant_obj) {
        extract_string(home_assistant_obj, "name", &config.home_assistant.name);
        extract_string(home_assistant_obj, "port", &config.home_assistant.port);
    }

    // Print the configuration
    print_config(&config);

    // Decrement the reference count of the root JSON object
    json_decref(root);

    return 0;
}
