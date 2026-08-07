#include "json.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* text;
    size_t size;
    size_t position;
    char* error;
    size_t error_size;
} Parser;

static void set_error(Parser* parser, const char* message) {
    if (parser->error_size != 0u && parser->error[0] == '\0') {
        snprintf(parser->error, parser->error_size, "%s at byte %zu", message,
                 parser->position);
    }
}

static void skip_whitespace(Parser* parser) {
    while (parser->position < parser->size &&
           isspace((unsigned char)parser->text[parser->position]) != 0) {
        parser->position++;
    }
}

static JsonValue* allocate_value(JsonType type) {
    JsonValue* value = calloc(1u, sizeof(*value));
    if (value != NULL) {
        value->type = type;
    }
    return value;
}

void json_free(JsonValue* value) {
    size_t index;
    if (value == NULL) {
        return;
    }
    if (value->type == JSON_STRING) {
        free(value->as.string);
    } else if (value->type == JSON_ARRAY) {
        for (index = 0u; index < value->as.array.count; index++) {
            json_free(value->as.array.items[index]);
        }
        free(value->as.array.items);
    } else if (value->type == JSON_OBJECT) {
        for (index = 0u; index < value->as.object.count; index++) {
            free(value->as.object.members[index].name);
            json_free(value->as.object.members[index].value);
        }
        free(value->as.object.members);
    }
    free(value);
}

static bool append_byte(char** text, size_t* length, size_t* capacity, char byte) {
    char* resized;
    if (*length + 1u >= *capacity) {
        size_t next = *capacity == 0u ? 32u : *capacity * 2u;
        resized = realloc(*text, next);
        if (resized == NULL) {
            return false;
        }
        *text = resized;
        *capacity = next;
    }
    (*text)[(*length)++] = byte;
    return true;
}

static bool append_utf8(char** text, size_t* length, size_t* capacity,
                        uint32_t codepoint) {
    if (codepoint <= 0x7fu) {
        return append_byte(text, length, capacity, (char)codepoint);
    }
    if (codepoint <= 0x7ffu) {
        return append_byte(text, length, capacity, (char)(0xc0u | (codepoint >> 6u))) &&
               append_byte(text, length, capacity, (char)(0x80u | (codepoint & 0x3fu)));
    }
    return append_byte(text, length, capacity, (char)(0xe0u | (codepoint >> 12u))) &&
           append_byte(text, length, capacity,
                       (char)(0x80u | ((codepoint >> 6u) & 0x3fu))) &&
           append_byte(text, length, capacity, (char)(0x80u | (codepoint & 0x3fu)));
}

static int hex_digit(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static char* parse_string(Parser* parser) {
    char* result = NULL;
    size_t length = 0u;
    size_t capacity = 0u;
    if (parser->position >= parser->size || parser->text[parser->position++] != '"') {
        set_error(parser, "expected string");
        return NULL;
    }
    while (parser->position < parser->size) {
        unsigned char byte = (unsigned char)parser->text[parser->position++];
        if (byte == '"') {
            if (!append_byte(&result, &length, &capacity, '\0')) {
                set_error(parser, "out of memory");
                free(result);
                return NULL;
            }
            return result;
        }
        if (byte < 0x20u) {
            set_error(parser, "invalid string character");
            free(result);
            return NULL;
        }
        if (byte == '\\') {
            uint32_t codepoint = 0u;
            size_t index;
            if (parser->position >= parser->size) {
                break;
            }
            byte = (unsigned char)parser->text[parser->position++];
            if (byte == 'u') {
                for (index = 0u; index < 4u; index++) {
                    int digit;
                    if (parser->position >= parser->size ||
                        (digit = hex_digit(parser->text[parser->position++])) < 0) {
                        set_error(parser, "invalid Unicode escape");
                        free(result);
                        return NULL;
                    }
                    codepoint = (codepoint << 4u) | (uint32_t)digit;
                }
                if (!append_utf8(&result, &length, &capacity, codepoint)) {
                    set_error(parser, "out of memory");
                    free(result);
                    return NULL;
                }
                continue;
            }
            switch (byte) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                byte = '\b';
                break;
            case 'f':
                byte = '\f';
                break;
            case 'n':
                byte = '\n';
                break;
            case 'r':
                byte = '\r';
                break;
            case 't':
                byte = '\t';
                break;
            default:
                set_error(parser, "invalid string escape");
                free(result);
                return NULL;
            }
        }
        if (!append_byte(&result, &length, &capacity, (char)byte)) {
            set_error(parser, "out of memory");
            free(result);
            return NULL;
        }
    }
    set_error(parser, "unterminated string");
    free(result);
    return NULL;
}

static JsonValue* parse_value(Parser* parser);

static bool reserve_array(JsonValue* value) {
    JsonValue** resized;
    size_t capacity;
    if (value->as.array.count < value->as.array.capacity) {
        return true;
    }
    capacity = value->as.array.capacity == 0u ? 8u : value->as.array.capacity * 2u;
    resized = realloc(value->as.array.items, capacity * sizeof(*resized));
    if (resized == NULL) {
        return false;
    }
    value->as.array.items = resized;
    value->as.array.capacity = capacity;
    return true;
}

static JsonValue* parse_array(Parser* parser) {
    JsonValue* value = allocate_value(JSON_ARRAY);
    if (value == NULL) {
        set_error(parser, "out of memory");
        return NULL;
    }
    parser->position++;
    skip_whitespace(parser);
    if (parser->position < parser->size && parser->text[parser->position] == ']') {
        parser->position++;
        return value;
    }
    for (;;) {
        JsonValue* item = parse_value(parser);
        if (item == NULL || !reserve_array(value)) {
            json_free(item);
            set_error(parser, "out of memory");
            json_free(value);
            return NULL;
        }
        value->as.array.items[value->as.array.count++] = item;
        skip_whitespace(parser);
        if (parser->position >= parser->size) {
            break;
        }
        if (parser->text[parser->position] == ']') {
            parser->position++;
            return value;
        }
        if (parser->text[parser->position++] != ',') {
            break;
        }
    }
    set_error(parser, "invalid array");
    json_free(value);
    return NULL;
}

static bool reserve_object(JsonValue* value) {
    JsonMember* resized;
    size_t capacity;
    if (value->as.object.count < value->as.object.capacity) {
        return true;
    }
    capacity = value->as.object.capacity == 0u ? 8u : value->as.object.capacity * 2u;
    resized = realloc(value->as.object.members, capacity * sizeof(*resized));
    if (resized == NULL) {
        return false;
    }
    value->as.object.members = resized;
    value->as.object.capacity = capacity;
    return true;
}

static JsonValue* parse_object(Parser* parser) {
    JsonValue* value = allocate_value(JSON_OBJECT);
    if (value == NULL) {
        set_error(parser, "out of memory");
        return NULL;
    }
    parser->position++;
    skip_whitespace(parser);
    if (parser->position < parser->size && parser->text[parser->position] == '}') {
        parser->position++;
        return value;
    }
    for (;;) {
        char* name;
        JsonValue* item;
        skip_whitespace(parser);
        name = parse_string(parser);
        skip_whitespace(parser);
        if (name == NULL || parser->position >= parser->size ||
            parser->text[parser->position++] != ':') {
            free(name);
            break;
        }
        item = parse_value(parser);
        if (item == NULL || !reserve_object(value)) {
            free(name);
            json_free(item);
            set_error(parser, "out of memory");
            json_free(value);
            return NULL;
        }
        value->as.object.members[value->as.object.count].name = name;
        value->as.object.members[value->as.object.count++].value = item;
        skip_whitespace(parser);
        if (parser->position >= parser->size) {
            break;
        }
        if (parser->text[parser->position] == '}') {
            parser->position++;
            return value;
        }
        if (parser->text[parser->position++] != ',') {
            break;
        }
    }
    set_error(parser, "invalid object");
    json_free(value);
    return NULL;
}

static JsonValue* parse_integer_value(Parser* parser) {
    JsonValue* value;
    char* end;
    long long integer;
    size_t start = parser->position;
    errno = 0;
    integer = strtoll(parser->text + start, &end, 10);
    if (errno != 0 || end == parser->text + start) {
        set_error(parser, "invalid integer");
        return NULL;
    }
    parser->position = (size_t)(end - parser->text);
    if (parser->position < parser->size && (parser->text[parser->position] == '.' ||
                                            parser->text[parser->position] == 'e' ||
                                            parser->text[parser->position] == 'E')) {
        set_error(parser, "floating point values are not supported");
        return NULL;
    }
    value = allocate_value(JSON_INTEGER);
    if (value == NULL) {
        set_error(parser, "out of memory");
        return NULL;
    }
    value->as.integer = (int64_t)integer;
    return value;
}

static JsonValue* parse_literal(Parser* parser, const char* literal, JsonType type,
                                bool boolean) {
    JsonValue* value;
    size_t length = strlen(literal);
    if (length > parser->size - parser->position ||
        memcmp(parser->text + parser->position, literal, length) != 0) {
        set_error(parser, "invalid literal");
        return NULL;
    }
    parser->position += length;
    value = allocate_value(type);
    if (value != NULL && type == JSON_BOOLEAN) {
        value->as.boolean = boolean;
    }
    if (value == NULL) {
        set_error(parser, "out of memory");
    }
    return value;
}

static JsonValue* parse_value(Parser* parser) {
    JsonValue* value;
    skip_whitespace(parser);
    if (parser->position >= parser->size) {
        set_error(parser, "expected value");
        return NULL;
    }
    switch (parser->text[parser->position]) {
    case '{':
        return parse_object(parser);
    case '[':
        return parse_array(parser);
    case '"':
        value = allocate_value(JSON_STRING);
        if (value != NULL) {
            value->as.string = parse_string(parser);
            if (value->as.string != NULL) {
                return value;
            }
        }
        json_free(value);
        return NULL;
    case 't':
        return parse_literal(parser, "true", JSON_BOOLEAN, true);
    case 'f':
        return parse_literal(parser, "false", JSON_BOOLEAN, false);
    case 'n':
        return parse_literal(parser, "null", JSON_NULL, false);
    default:
        return parse_integer_value(parser);
    }
}

JsonValue* json_parse(const char* text, size_t size, char* error, size_t error_size) {
    Parser parser = {text, size, 0u, error, error_size};
    JsonValue* value;
    if (error_size != 0u) {
        error[0] = '\0';
    }
    value = parse_value(&parser);
    skip_whitespace(&parser);
    if (value != NULL && parser.position != parser.size) {
        set_error(&parser, "unexpected trailing data");
        json_free(value);
        return NULL;
    }
    return value;
}

JsonValue* json_read(const char* path, char* error, size_t error_size) {
    FILE* file = fopen(path, "rb");
    char* text;
    long length;
    JsonValue* value;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        snprintf(error, error_size, "cannot open JSON file: %s", path);
        return NULL;
    }
    text = malloc((size_t)length + 1u);
    if (text == NULL || fread(text, 1u, (size_t)length, file) != (size_t)length) {
        fclose(file);
        free(text);
        snprintf(error, error_size, "cannot read JSON file: %s", path);
        return NULL;
    }
    fclose(file);
    text[length] = '\0';
    value = json_parse(text, (size_t)length, error, error_size);
    free(text);
    return value;
}

const JsonValue* json_get(const JsonValue* object, const char* name) {
    size_t index;
    if (object == NULL || object->type != JSON_OBJECT) {
        return NULL;
    }
    for (index = 0u; index < object->as.object.count; index++) {
        if (strcmp(object->as.object.members[index].name, name) == 0) {
            return object->as.object.members[index].value;
        }
    }
    return NULL;
}

const char* json_string(const JsonValue* value) {
    return value != NULL && value->type == JSON_STRING ? value->as.string : NULL;
}

bool json_integer(const JsonValue* value, int64_t* result) {
    if (value == NULL || value->type != JSON_INTEGER) {
        return false;
    }
    *result = value->as.integer;
    return true;
}

bool json_boolean(const JsonValue* value, bool* result) {
    if (value == NULL || value->type != JSON_BOOLEAN) {
        return false;
    }
    *result = value->as.boolean;
    return true;
}
