#include "scenario_stream.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char** items;
    size_t count;
    size_t capacity;
} PathList;

static bool append_path(PathList* paths, const char* path) {
    char** resized;
    char* copy;
    if (paths->count == paths->capacity) {
        size_t capacity = paths->capacity == 0u ? 16u : paths->capacity * 2u;
        resized = realloc(paths->items, capacity * sizeof(*resized));
        if (resized == NULL) {
            return false;
        }
        paths->items = resized;
        paths->capacity = capacity;
    }
    copy = malloc(strlen(path) + 1u);
    if (copy == NULL) {
        return false;
    }
    strcpy(copy, path);
    paths->items[paths->count++] = copy;
    return true;
}

static void free_paths(PathList* paths) {
    size_t index;
    for (index = 0u; index < paths->count; index++) {
        free(paths->items[index]);
    }
    free(paths->items);
}

static int compare_paths(const void* left, const void* right) {
    return _stricmp(*(const char* const*)left, *(const char* const*)right);
}

static bool collect_paths(const char* pattern, PathList* paths, char* error,
                          size_t error_size) {
    struct _finddata_t entry;
    intptr_t handle;
    const char* slash = strrchr(pattern, '\\');
    const char* forward = strrchr(pattern, '/');
    size_t prefix_length;
    if (forward != NULL && (slash == NULL || forward > slash)) {
        slash = forward;
    }
    prefix_length = slash == NULL ? 0u : (size_t)(slash - pattern + 1u);
    handle = _findfirst(pattern, &entry);
    if (handle == -1) {
        snprintf(error, error_size, "scenario pattern matched no files: %s", pattern);
        return false;
    }
    do {
        char* path;
        size_t length;
        if ((entry.attrib & _A_SUBDIR) != 0) {
            continue;
        }
        length = prefix_length + strlen(entry.name);
        path = malloc(length + 1u);
        if (path == NULL) {
            _findclose(handle);
            snprintf(error, error_size, "out of memory");
            return false;
        }
        memcpy(path, pattern, prefix_length);
        strcpy(path + prefix_length, entry.name);
        if (!append_path(paths, path)) {
            free(path);
            _findclose(handle);
            snprintf(error, error_size, "out of memory");
            return false;
        }
        free(path);
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
    qsort(paths->items, paths->count, sizeof(*paths->items), compare_paths);
    return true;
}

static char* read_text(const char* path, size_t* size, char* error, size_t error_size) {
    FILE* file = fopen(path, "rb");
    char* text;
    long length;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        snprintf(error, error_size, "cannot open scenario file: %s", path);
        return NULL;
    }
    text = malloc((size_t)length + 1u);
    if (text == NULL || fread(text, 1u, (size_t)length, file) != (size_t)length) {
        fclose(file);
        free(text);
        snprintf(error, error_size, "cannot read scenario file: %s", path);
        return NULL;
    }
    fclose(file);
    text[length] = '\0';
    *size = (size_t)length;
    return text;
}

static size_t skip_space(const char* text, size_t size, size_t position) {
    while (position < size && (text[position] == ' ' || text[position] == '\t' ||
                               text[position] == '\r' || text[position] == '\n')) {
        position++;
    }
    return position;
}

static size_t scenarios_start(const char* text, size_t size) {
    const char key[] = "\"scenarios\"";
    const char* found = strstr(text, key);
    size_t position;
    if (found == NULL) {
        return size;
    }
    position = (size_t)(found - text) + sizeof(key) - 1u;
    position = skip_space(text, size, position);
    if (position >= size || text[position++] != ':') {
        return size;
    }
    position = skip_space(text, size, position);
    return position < size && text[position] == '[' ? position + 1u : size;
}

static size_t object_end(const char* text, size_t size, size_t start) {
    size_t position;
    unsigned depth = 0u;
    bool string = false;
    bool escaped = false;
    for (position = start; position < size; position++) {
        char byte = text[position];
        if (string) {
            if (escaped) {
                escaped = false;
            } else if (byte == '\\') {
                escaped = true;
            } else if (byte == '"') {
                string = false;
            }
            continue;
        }
        if (byte == '"') {
            string = true;
        } else if (byte == '{') {
            depth++;
        } else if (byte == '}') {
            if (--depth == 0u) {
                return position + 1u;
            }
        }
    }
    return size;
}

static bool visit_file(const char* path, ScenarioVisitor visitor, void* context,
                       char* error, size_t error_size) {
    size_t size;
    char* text = read_text(path, &size, error, error_size);
    size_t position;
    if (text == NULL) {
        return false;
    }
    position = scenarios_start(text, size);
    while (position < size) {
        size_t end;
        JsonValue* scenario;
        position = skip_space(text, size, position);
        if (position < size && text[position] == ',') {
            position = skip_space(text, size, position + 1u);
        }
        if (position < size && text[position] == ']') {
            free(text);
            return true;
        }
        if (position >= size || text[position] != '{' ||
            (end = object_end(text, size, position)) == size) {
            snprintf(error, error_size, "invalid scenario array in %s", path);
            free(text);
            return false;
        }
        scenario = json_parse(text + position, end - position, error, error_size);
        if (scenario == NULL || !visitor(path, scenario, context, error, error_size)) {
            json_free(scenario);
            free(text);
            return false;
        }
        json_free(scenario);
        position = end;
    }
    snprintf(error, error_size, "unterminated scenario array in %s", path);
    free(text);
    return false;
}

bool scenario_stream(const char* pattern, ScenarioVisitor visitor, void* context,
                     char* error, size_t error_size) {
    PathList paths = {0};
    size_t index;
    bool result = collect_paths(pattern, &paths, error, error_size);
    for (index = 0u; result && index < paths.count; index++) {
        result = visit_file(paths.items[index], visitor, context, error, error_size);
    }
    free_paths(&paths);
    return result;
}
