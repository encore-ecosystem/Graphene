/* Graphene-specific native integrations live here. Vulkan is supplied by
 * Luma's vulkan_native workspace package. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct {
    size_t ref_count;
    size_t len;
    char data[];
} graphene_str_object;

typedef struct {
    graphene_str_object *object;
} encore_str;

extern void *encore_str_from_cstr(const char *value);

static encore_str graphene_string(const char *value) {
    encore_str result = {
        (graphene_str_object *)encore_str_from_cstr(value == NULL ? "" : value)
    };
    return result;
}

static char *graphene_c_string(encore_str value) {
    size_t length = value.object == NULL ? 0 : value.object->len;
    char *result = (char *)malloc(length + 1);
    if (result == NULL) return NULL;
    if (length > 0) memcpy(result, value.object->data, length);
    result[length] = '\0';
    return result;
}

uint64_t graphene_perf_counter_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter) || frequency.QuadPart <= 0) {
        return 0;
    }
    uint64_t ticks = (uint64_t)counter.QuadPart;
    uint64_t hz = (uint64_t)frequency.QuadPart;
    return (ticks / hz) * 1000000000ull +
        ((ticks % hz) * 1000000000ull) / hz;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
    return (uint64_t)value.tv_sec * 1000000000ull +
        (uint64_t)value.tv_nsec;
#endif
}

#include "workspace/automation_native.inc"
