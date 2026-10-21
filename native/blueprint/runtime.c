/* Copied into an immutable generated package. Only abi.h is shared with hosts. */
#include "abi.h"
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct { size_t refs, length; char data[]; } gbp_string_object;
typedef struct { gbp_string_object *object; } gbp_string;
extern gbp_string encore_str_from_buffer(const char *, size_t);
extern void *encore_str_from_cstr(const char *);

typedef void (*gbp_function)(uintptr_t, uintptr_t, float);
typedef struct { uint64_t id; uint32_t type; } gbp_property;
typedef struct { const char *kind, *action; gbp_function function; } gbp_event;
typedef struct {
    const char *id;
    size_t property_count;
    const gbp_property *properties;
    size_t event_count;
    const gbp_event *events;
    gbp_function initialize;
} gbp_class;
typedef struct {
    const gbp_class *class_info;
    gbp_value *properties;
    const char *error;
    uint64_t node;
    bool constructing, begun;
} gbp_state;

static void gbp_fail(gbp_state *state, uint64_t node, const char *message) {
    if (state && !state->error) { state->error = message; state->node = node; }
}
bool gbp_ok(uintptr_t state) { return state && !((gbp_state *)state)->error; }
static gbp_value *gbp_slot(uintptr_t handle, uint32_t slot) {
    gbp_state *state = (gbp_state *)handle;
    if (!state || slot >= state->class_info->property_count) {
        gbp_fail(state, 0, "property slot is out of range"); return NULL;
    }
    return &state->properties[slot];
}
bool gbp_get_bool(uintptr_t state, uint32_t slot) { gbp_value *v = gbp_slot(state, slot); return v && v->boolean; }
int32_t gbp_get_i32(uintptr_t state, uint32_t slot) { gbp_value *v = gbp_slot(state, slot); return v ? v->integer : 0; }
uint64_t gbp_get_id(uintptr_t state, uint32_t slot) { gbp_value *v = gbp_slot(state, slot); return v ? v->reference : 0; }
float gbp_get_f32(uintptr_t state, uint32_t slot, uint32_t axis) {
    gbp_value *v = gbp_slot(state, slot); return v && axis < 3 ? v->vector[axis] : 0;
}
gbp_string gbp_get_string(uintptr_t state, uint32_t slot) {
    gbp_value *v = gbp_slot(state, slot);
    return encore_str_from_buffer(v && v->bytes ? v->bytes : "", v ? v->length : 0);
}
void gbp_set_bool(uintptr_t state, uint32_t slot, bool value) { gbp_value *v = gbp_slot(state, slot); if (v) v->boolean = value; }
void gbp_set_i32(uintptr_t state, uint32_t slot, int32_t value) { gbp_value *v = gbp_slot(state, slot); if (v) v->integer = value; }
void gbp_set_id(uintptr_t state, uint32_t slot, uint64_t value) { gbp_value *v = gbp_slot(state, slot); if (v) v->reference = value; }
void gbp_set_f32(uintptr_t state, uint32_t slot, uint32_t axis, float value) {
    gbp_value *v = gbp_slot(state, slot);
    if (!isfinite(value) || axis >= 3) { gbp_fail((gbp_state *)state, 0, "nonfinite property value"); return; }
    if (v) v->vector[axis] = value;
}
static bool gbp_copy_bytes(gbp_value *value, const char *bytes, size_t length) {
    if (length > 1048576 || (length && !bytes)) return false;
    char *copy = malloc(length + 1);
    if (!copy) return false;
    if (length) memcpy(copy, bytes, length);
    copy[length] = '\0';
    free((void *)value->bytes); value->bytes = copy; value->length = length;
    return true;
}
void gbp_set_string(uintptr_t state, uint32_t slot, gbp_string string) {
    gbp_value *value = gbp_slot(state, slot);
    if (value && !gbp_copy_bytes(value, string.object ? string.object->data : "", string.object ? string.object->length : 0))
        gbp_fail((gbp_state *)state, 0, "string property allocation failed or exceeds 1 MiB");
}
static const gbp_host_v1 *gbp_host(uintptr_t state, uintptr_t host, uint64_t node) {
    const gbp_host_v1 *api = (const gbp_host_v1 *)host;
    if (!api || api->abi != GBP_ABI_VERSION || api->size < sizeof(*api)) {
        gbp_fail((gbp_state *)state, node, "host ABI mismatch"); return NULL;
    }
    return api;
}
float gbp_transform(uintptr_t state, uintptr_t host, uint64_t component, uint32_t field, uint32_t axis, uint64_t node) {
    const gbp_host_v1 *api = gbp_host(state, host, node); float value = 0;
    if (api && (!api->read_transform || !api->read_transform(api->context, component, field, axis, &value)))
        gbp_fail((gbp_state *)state, node, "transform target is missing");
    return value;
}
bool gbp_visible(uintptr_t state, uintptr_t host, uint64_t component, uint64_t node) {
    const gbp_host_v1 *api = gbp_host(state, host, node); bool value = false;
    if (api && (!api->read_visible || !api->read_visible(api->context, component, &value)))
        gbp_fail((gbp_state *)state, node, "visibility target is missing");
    return value;
}
void gbp_set_visible(uintptr_t state, uintptr_t host, uint64_t component, bool value, uint64_t node) {
    if (!gbp_ok(state)) return;
    const gbp_host_v1 *api = gbp_host(state, host, node);
    if (api && (!api->write_visible || !api->write_visible(api->context, component, value)))
        gbp_fail((gbp_state *)state, node, "visibility target is missing");
}
void gbp_set_transform(uintptr_t state, uintptr_t host, uint64_t component, uint32_t field, float x, float y, float z, bool add, uint64_t node) {
    if (!gbp_ok(state)) return;
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) { gbp_fail((gbp_state *)state, node, "nonfinite transform"); return; }
    const gbp_host_v1 *api = gbp_host(state, host, node); const float vector[3] = {x, y, z};
    if (api && (!api->write_transform || !api->write_transform(api->context, component, field, vector, add)))
        gbp_fail((gbp_state *)state, node, "transform target is missing");
}
void gbp_print(uintptr_t state, uintptr_t host, gbp_string string, uint64_t node) {
    if (!gbp_ok(state)) return;
    const gbp_host_v1 *api = gbp_host(state, host, node);
    if (api && (!api->print || !api->print(api->context, string.object ? string.object->data : "", string.object ? string.object->length : 0)))
        gbp_fail((gbp_state *)state, node, "host log rejected output");
}
int32_t gbp_div_i32(uintptr_t state, int32_t a, int32_t b, uint64_t node) {
    if (!b || (a == INT32_MIN && b == -1)) { gbp_fail((gbp_state *)state, node, "invalid integer division"); return 0; }
    return a / b;
}
float gbp_div_f32(uintptr_t state, float a, float b, uint64_t node) {
    if (b == 0 || !isfinite(a / b)) { gbp_fail((gbp_state *)state, node, "invalid floating point division"); return 0; }
    return a / b;
}

#include "registry.h"

static const char *gbp_class_id(size_t index, size_t *length) {
    if (index >= GBP_CLASS_COUNT || !length) return NULL;
    *length = strlen(gbp_classes[index].id); return gbp_classes[index].id;
}
static void gbp_destroy(void *handle) {
    gbp_state *state = handle;
    if (!state) return;
    for (size_t i = 0; i < state->class_info->property_count; ++i) free((void *)state->properties[i].bytes);
    free(state->properties); free(state);
}
static void *gbp_create(size_t index) {
    if (index >= GBP_CLASS_COUNT) return NULL;
    gbp_state *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->class_info = &gbp_classes[index];
    state->properties = calloc(state->class_info->property_count + 1, sizeof(*state->properties));
    if (!state->properties) { free(state); return NULL; }
    for (size_t i = 0; i < state->class_info->property_count; ++i) state->properties[i].type = state->class_info->properties[i].type;
    state->class_info->initialize((uintptr_t)state, 0, 0);
    if (state->error) { gbp_destroy(state); return NULL; }
    return state;
}
static gbp_value *gbp_find_property(gbp_state *state, uint64_t id) {
    if (!state) return NULL;
    for (size_t i = 0; i < state->class_info->property_count; ++i)
        if (state->class_info->properties[i].id == id) return &state->properties[i];
    return NULL;
}
static bool gbp_set_property(void *handle, uint64_t id, const gbp_value *input) {
    gbp_value *value = gbp_find_property(handle, id);
    if (!value || !input || value->type != input->type) return false;
    if (input->type == 4) return gbp_copy_bytes(value, input->bytes, input->length);
    if (input->type == 3 && !isfinite(input->vector[0])) return false;
    if (input->type == 5 && (!isfinite(input->vector[0]) || !isfinite(input->vector[1]) || !isfinite(input->vector[2]))) return false;
    *value = *input; value->bytes = NULL; value->length = 0;
    return true;
}
static bool gbp_get_property(void *handle, uint64_t id, gbp_value *output) {
    gbp_value *value = gbp_find_property(handle, id);
    if (!value || !output) return false;
    *output = *value; return true;
}
static bool gbp_equal_slice(const char *text, const char *bytes, size_t length) {
    return text && strlen(text) == length && (!length || (bytes && memcmp(text, bytes, length) == 0));
}
static bool gbp_invoke(void *handle, const gbp_host_v1 *host, const char *kind, size_t kind_length, const char *action, size_t action_length, float delta) {
    gbp_state *state = handle;
    if (!state || state->error || !host || host->abi != GBP_ABI_VERSION || host->size < sizeof(*host) || !isfinite(delta) || delta < 0) return false;
    bool construction = gbp_equal_slice("construction", kind, kind_length);
    bool begin = gbp_equal_slice("begin-play", kind, kind_length);
    if (construction) { if (state->constructing || state->begun) return false; state->constructing = true; }
    else if (begin) { if (state->begun || !state->constructing) return false; state->begun = true; }
    else if (!state->begun) return false;
    if (!construction && !begin && !gbp_equal_slice("tick", kind, kind_length) &&
        !gbp_equal_slice("input-pressed", kind, kind_length) && !gbp_equal_slice("input-released", kind, kind_length)) return false;
    for (size_t i = 0; i < state->class_info->event_count; ++i) {
        const gbp_event *event = &state->class_info->events[i];
        if (gbp_equal_slice(event->kind, kind, kind_length) && gbp_equal_slice(event->action, action, action_length)) {
            event->function((uintptr_t)state, (uintptr_t)host, delta);
            if (state->error) return false;
        }
    }
    return true;
}
static const char *gbp_error(void *handle, uint64_t *node, size_t *length) {
    gbp_state *state = handle;
    if (!node || !length) return NULL;
    *node = state ? state->node : 0;
    const char *message = state && state->error ? state->error : "";
    *length = strlen(message); return message;
}
static const gbp_api_v1 gbp_api = { GBP_ABI_VERSION, sizeof(gbp_api_v1), GBP_TARGET, GBP_CLASS_COUNT,
    gbp_class_id, gbp_create, gbp_destroy, gbp_set_property, gbp_get_property, gbp_invoke, gbp_error };
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
const gbp_api_v1 *graphene_blueprint_api_v1(void) { return &gbp_api; }
