#ifndef DSPIC33_SIM_JSON_H
#define DSPIC33_SIM_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_INTEGER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

typedef struct {
    char* name;
    JsonValue* value;
} JsonMember;

struct JsonValue {
    JsonType type;
    union {
        bool boolean;
        int64_t integer;
        char* string;
        struct {
            JsonValue** items;
            size_t count;
            size_t capacity;
        } array;
        struct {
            JsonMember* members;
            size_t count;
            size_t capacity;
        } object;
    } as;
};

JsonValue* json_parse(const char* text, size_t size, char* error, size_t error_size);
JsonValue* json_read(const char* path, char* error, size_t error_size);
void json_free(JsonValue* value);
const JsonValue* json_get(const JsonValue* object, const char* name);
const char* json_string(const JsonValue* value);
bool json_integer(const JsonValue* value, int64_t* result);
bool json_boolean(const JsonValue* value, bool* result);

#endif
