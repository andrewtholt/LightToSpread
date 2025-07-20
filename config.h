#ifndef CONFIG_H
#define CONFIG_H

#include <jansson.h>

// Structs for the JSON structure from spreadToMysql.json
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
    char *name;
    char *port;
    char *user;
    char *default_group;
} SpreadConfig;

typedef struct {
    RemoteConfig remote;
    LocalConfig local;
    CommonConfig common;
    DatabaseConfig database;
    SocketConfig socket;
    TopicsConfig topics;
    LocationConfig location;
    HomeAssistantConfig home_assistant;
    SpreadConfig spread;
    int verbose;
    int buffer_size;
    int max_pending_connections;
} BridgeConfig;

// Helper function to extract string value from a json object
static inline void extract_string(json_t *parent, const char *key, char **target) {
    json_t *obj = json_object_get(parent, key);
    if (json_is_string(obj)) {
        *target = strdup(json_string_value(obj));
    }
}

#endif // CONFIG_H
