#ifndef DSPIC33E_SIM_SCENARIO_STREAM_H
#define DSPIC33E_SIM_SCENARIO_STREAM_H

#include <stdbool.h>
#include <stddef.h>

#include "json.h"

typedef bool (*ScenarioVisitor)(const char* path, const JsonValue* scenario,
                                void* context, char* error, size_t error_size);

bool scenario_stream(const char* pattern, ScenarioVisitor visitor, void* context,
                     char* error, size_t error_size);

#endif
