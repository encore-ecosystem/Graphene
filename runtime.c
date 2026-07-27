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

#include "workspace/automation_native.inc"
