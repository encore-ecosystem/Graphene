/* Host adapter. Module allocation and Encore graph runtimes never cross the ABI. */
#include "abi.h"
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <dlfcn.h>
#include <pthread.h>
#endif

/* Handles are plain integers and can legally reach another Encore worker.
 * Keep validation and use in one critical section, including module unload.
 * Host callbacks below run within it and never reenter the public registry. */
#if defined(_WIN32)
static SRWLOCK graphene_bp_mutex = SRWLOCK_INIT;
static void graphene_bp_lock(void) { AcquireSRWLockExclusive(&graphene_bp_mutex); }
static void graphene_bp_unlock(void) { ReleaseSRWLockExclusive(&graphene_bp_mutex); }
#else
static pthread_mutex_t graphene_bp_mutex = PTHREAD_MUTEX_INITIALIZER;
static void graphene_bp_lock(void) { pthread_mutex_lock(&graphene_bp_mutex); }
static void graphene_bp_unlock(void) { pthread_mutex_unlock(&graphene_bp_mutex); }
#endif

typedef struct graphene_bp_module {
    struct graphene_bp_module *next;
    void *library;
    const gbp_api_v1 *api;
    size_t instances;
    uintptr_t token;
} graphene_bp_module;
typedef struct {
    uint64_t id, parent;
    float transform[3][3];
    bool visible;
} graphene_bp_component;
typedef struct graphene_bp_instance {
    struct graphene_bp_instance *next;
    graphene_bp_module *module;
    void *state;
    gbp_host_v1 host;
    graphene_bp_component *components;
    size_t component_count, component_capacity;
    char log[65537];
    size_t log_length;
    uintptr_t token;
} graphene_bp_instance;

static graphene_bp_module *graphene_bp_modules;
static graphene_bp_instance *graphene_bp_instances;
static _Thread_local char graphene_bp_last_error[1024];
static uintptr_t graphene_bp_next_token = 1;
encore_str graphene_bp_target(void) { return graphene_string(GBP_TARGET); }
encore_str graphene_bp_stage(encore_str parent) {
    char *base = graphene_c_string(parent);
    if (!base) return graphene_string("");
    size_t length = strlen(base);
    char *path = malloc(length + 64);
    if (!path) { free(base); return graphene_string(""); }
#if defined(_WIN32)
    static LONG counter;
    bool created = false;
    for (size_t attempt = 0; attempt < 1024; ++attempt) {
        snprintf(path, length + 64, "%s/build-%lu-%lu", base, (unsigned long)GetCurrentProcessId(), (unsigned long)InterlockedIncrement(&counter));
        if (CreateDirectoryA(path, NULL)) { created = true; break; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    if (!created) path[0] = '\0';
#else
    snprintf(path, length + 64, "%s/build-XXXXXX", base);
    if (!mkdtemp(path)) path[0] = '\0';
#endif
    encore_str result = graphene_string(path); free(path); free(base); return result;
}
static void graphene_bp_error(const char *message) {
    size_t count = message ? strlen(message) : 0;
    if (count >= sizeof(graphene_bp_last_error)) count = sizeof(graphene_bp_last_error) - 1;
    if (count) memcpy(graphene_bp_last_error, message, count);
    graphene_bp_last_error[count] = '\0';
}
static graphene_bp_module *graphene_bp_module_find(uintptr_t handle) {
    for (graphene_bp_module *m = graphene_bp_modules; m; m = m->next) if (m->token == handle) return m;
    return NULL;
}
static graphene_bp_instance *graphene_bp_instance_find(uintptr_t handle) {
    for (graphene_bp_instance *i = graphene_bp_instances; i; i = i->next) if (i->token == handle) return i;
    return NULL;
}
static graphene_bp_component *graphene_bp_component_find(graphene_bp_instance *instance, uint64_t id) {
    if (instance) for (size_t i = 0; i < instance->component_count; ++i) if (instance->components[i].id == id) return &instance->components[i];
    return NULL;
}
static bool graphene_bp_read_transform(void *context, uint64_t id, uint32_t field, uint32_t axis, float *value) {
    graphene_bp_component *component = graphene_bp_component_find(context, id);
    if (!component || field >= 3 || axis >= 3 || !value) return false;
    *value = component->transform[field][axis]; return true;
}
static bool graphene_bp_write_transform(void *context, uint64_t id, uint32_t field, const float *value, bool add) {
    graphene_bp_component *component = graphene_bp_component_find(context, id);
    if (!component || field >= 3 || !value) return false;
    float next[3];
    for (size_t axis = 0; axis < 3; ++axis) {
        next[axis] = value[axis] + (add ? component->transform[field][axis] : 0);
        if (!isfinite(next[axis])) return false;
    }
    memcpy(component->transform[field], next, sizeof(next)); return true;
}
static bool graphene_bp_read_visible(void *context, uint64_t id, bool *value) {
    graphene_bp_component *component = graphene_bp_component_find(context, id);
    if (!component || !value) return false;
    *value = component->visible; return true;
}
static bool graphene_bp_write_visible(void *context, uint64_t id, bool value) {
    graphene_bp_component *component = graphene_bp_component_find(context, id);
    if (!component) return false;
    component->visible = value; return true;
}
static bool graphene_bp_log(void *context, const char *bytes, size_t length) {
    graphene_bp_instance *instance = context;
    if (!instance || length > 65536 || (length && !bytes)) return false;
    if (instance->log_length + length + 1 > 65536) return false;
    if (length) memcpy(instance->log + instance->log_length, bytes, length);
    instance->log_length += length; instance->log[instance->log_length++] = '\n';
    instance->log[instance->log_length] = '\0'; return true;
}
static void graphene_bp_library_close(void *library) {
#if defined(_WIN32)
    FreeLibrary((HMODULE)library);
#else
    dlclose(library);
#endif
}
static uintptr_t graphene_bp_open_impl(encore_str path) {
    graphene_bp_error("");
    char *name = graphene_c_string(path);
    if (!name) { graphene_bp_error("module path allocation failed"); return 0; }
    void *library = NULL;
    gbp_entry_v1 entry = NULL;
#if defined(_WIN32)
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    wchar_t *wide = count > 0 ? malloc((size_t)count * sizeof(*wide)) : NULL;
    if (wide && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, count))
        library = (void *)LoadLibraryExW(wide, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    free(wide);
    if (library) entry = (gbp_entry_v1)(uintptr_t)GetProcAddress((HMODULE)library, "graphene_blueprint_api_v1");
#else
    library = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (library) { void *symbol = dlsym(library, "graphene_blueprint_api_v1"); memcpy(&entry, &symbol, sizeof(entry)); }
#endif
    free(name);
    if (!library || !entry) {
        graphene_bp_error("unable to load Blueprint module or its v1 entry point");
        if (library) graphene_bp_library_close(library);
        return 0;
    }
    const gbp_api_v1 *api = entry();
    if (!api || api->abi != GBP_ABI_VERSION || api->size < sizeof(*api) || !api->target || strcmp(api->target, GBP_TARGET) ||
        !api->class_id || !api->create || !api->destroy || !api->invoke || !api->set_property || !api->get_property || !api->error) {
        graphene_bp_error("Blueprint target or host ABI mismatch"); graphene_bp_library_close(library); return 0;
    }
    graphene_bp_module *module = calloc(1, sizeof(*module));
    if (!module) { graphene_bp_library_close(library); graphene_bp_error("module allocation failed"); return 0; }
    if (!graphene_bp_next_token) { free(module); graphene_bp_library_close(library); graphene_bp_error("module handle space exhausted"); return 0; }
    module->token = graphene_bp_next_token++;
    module->api = api; module->library = library; module->next = graphene_bp_modules; graphene_bp_modules = module;
    return module->token;
}
static bool graphene_bp_close_impl(uintptr_t handle) {
    graphene_bp_module *module = graphene_bp_module_find(handle);
    if (!module || module->instances) { graphene_bp_error("module still has live instances or is already closed"); return false; }
    graphene_bp_module **link = &graphene_bp_modules;
    while (*link != module) link = &(*link)->next;
    *link = module->next; graphene_bp_library_close(module->library); free(module); return true;
}
encore_str graphene_bp_error_text(void) { return graphene_string(graphene_bp_last_error); }
static uintptr_t graphene_bp_create_impl(uintptr_t handle, encore_str class_id) {
    graphene_bp_module *module = graphene_bp_module_find(handle);
    if (!module || !class_id.object) return 0;
    size_t index;
    for (index = 0; index < module->api->class_count; ++index) {
        size_t length = 0; const char *id = module->api->class_id(index, &length);
        if (id && length == class_id.object->len && !memcmp(id, class_id.object->data, length)) break;
    }
    if (index == module->api->class_count) { graphene_bp_error("Blueprint class is not in the compiled snapshot"); return 0; }
    graphene_bp_instance *instance = calloc(1, sizeof(*instance));
    if (!instance) return 0;
    if (!graphene_bp_next_token) { free(instance); return 0; }
    instance->token = graphene_bp_next_token++;
    instance->state = module->api->create(index);
    if (!instance->state) { free(instance); graphene_bp_error("module instance initialization failed"); return 0; }
    instance->module = module;
    instance->host = (gbp_host_v1){ GBP_ABI_VERSION, sizeof(gbp_host_v1), instance, graphene_bp_read_transform,
        graphene_bp_write_transform, graphene_bp_read_visible, graphene_bp_write_visible, graphene_bp_log };
    instance->next = graphene_bp_instances; graphene_bp_instances = instance; ++module->instances;
    return instance->token;
}
static bool graphene_bp_destroy_impl(uintptr_t handle) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    if (!instance) return false;
    graphene_bp_instance **link = &graphene_bp_instances;
    while (*link != instance) link = &(*link)->next;
    *link = instance->next;
    instance->module->api->destroy(instance->state); --instance->module->instances;
    free(instance->components); free(instance); return true;
}
static bool graphene_bp_add_component_impl(uintptr_t handle, uint64_t id, uint64_t parent, float px, float py, float pz,
    float rx, float ry, float rz, float sx, float sy, float sz, bool visible) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    if (!instance || graphene_bp_component_find(instance, id)) return false;
    const float transform[3][3] = {{px, py, pz}, {rx, ry, rz}, {sx, sy, sz}};
    for (size_t f = 0; f < 3; ++f) for (size_t a = 0; a < 3; ++a) if (!isfinite(transform[f][a])) return false;
    if (instance->component_count == instance->component_capacity) {
        size_t capacity = instance->component_capacity ? instance->component_capacity * 2 : 8;
        void *buffer = realloc(instance->components, capacity * sizeof(*instance->components));
        if (!buffer) return false;
        instance->components = buffer; instance->component_capacity = capacity;
    }
    graphene_bp_component *component = &instance->components[instance->component_count++];
    component->id = id; component->parent = parent; component->visible = visible;
    memcpy(component->transform, transform, sizeof(transform)); return true;
}
static float graphene_bp_axis_impl(uintptr_t handle, uint64_t id, uint32_t field, uint32_t axis) {
    float value = 0; graphene_bp_read_transform(graphene_bp_instance_find(handle), id, field, axis, &value); return value;
}
static bool graphene_bp_has_component_impl(uintptr_t handle, uint64_t id) { return graphene_bp_component_find(graphene_bp_instance_find(handle), id) != NULL; }
static bool graphene_bp_is_visible_impl(uintptr_t handle, uint64_t id) {
    bool value = false; graphene_bp_read_visible(graphene_bp_instance_find(handle), id, &value); return value;
}
static bool graphene_bp_property_impl(uintptr_t handle, uint64_t id, uint32_t type, bool boolean, int32_t integer,
    float x, float y, float z, uint64_t reference, encore_str string) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    gbp_value value = {0}; value.type = type; value.boolean = boolean; value.integer = integer;
    value.vector[0] = x; value.vector[1] = y; value.vector[2] = z; value.reference = reference;
    value.bytes = string.object ? string.object->data : ""; value.length = string.object ? string.object->len : 0;
    return instance && instance->module->api->set_property(instance->state, id, &value);
}
static bool graphene_bp_invoke_impl(uintptr_t handle, encore_str kind, encore_str action, float delta) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    if (!instance || !kind.object) return false;
    return instance->module->api->invoke(instance->state, &instance->host, kind.object->data, kind.object->len,
        action.object ? action.object->data : "", action.object ? action.object->len : 0, delta);
}
static encore_str graphene_bp_instance_error_impl(uintptr_t handle) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    if (!instance) return graphene_string("Blueprint instance was destroyed");
    uint64_t node = 0; size_t length = 0;
    const char *error = instance->module->api->error(instance->state, &node, &length);
    return graphene_string(error);
}
static uint64_t graphene_bp_error_node_impl(uintptr_t handle) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    uint64_t node = 0; size_t length = 0;
    if (instance) instance->module->api->error(instance->state, &node, &length);
    return node;
}
static encore_str graphene_bp_take_log_impl(uintptr_t handle) {
    graphene_bp_instance *instance = graphene_bp_instance_find(handle);
    if (!instance) return graphene_string("");
    encore_str result = graphene_string(instance->log); instance->log_length = 0; instance->log[0] = '\0'; return result;
}

#define GBP_LOCKED(type, name, signature, arguments) \
    type graphene_bp_##name signature { \
        graphene_bp_lock(); \
        type result = graphene_bp_##name##_impl arguments; \
        graphene_bp_unlock(); \
        return result; \
    }
GBP_LOCKED(uintptr_t, open, (encore_str path), (path))
GBP_LOCKED(bool, close, (uintptr_t handle), (handle))
GBP_LOCKED(uintptr_t, create, (uintptr_t handle, encore_str class_id), (handle, class_id))
GBP_LOCKED(bool, destroy, (uintptr_t handle), (handle))
GBP_LOCKED(bool, add_component, (uintptr_t handle, uint64_t id, uint64_t parent, float px, float py, float pz,
    float rx, float ry, float rz, float sx, float sy, float sz, bool visible),
    (handle, id, parent, px, py, pz, rx, ry, rz, sx, sy, sz, visible))
GBP_LOCKED(float, axis, (uintptr_t handle, uint64_t id, uint32_t field, uint32_t axis), (handle, id, field, axis))
GBP_LOCKED(bool, has_component, (uintptr_t handle, uint64_t id), (handle, id))
GBP_LOCKED(bool, is_visible, (uintptr_t handle, uint64_t id), (handle, id))
GBP_LOCKED(bool, property, (uintptr_t handle, uint64_t id, uint32_t type, bool boolean, int32_t integer,
    float x, float y, float z, uint64_t reference, encore_str string), (handle, id, type, boolean, integer, x, y, z, reference, string))
GBP_LOCKED(bool, invoke, (uintptr_t handle, encore_str kind, encore_str action, float delta), (handle, kind, action, delta))
GBP_LOCKED(encore_str, instance_error, (uintptr_t handle), (handle))
GBP_LOCKED(uint64_t, error_node, (uintptr_t handle), (handle))
GBP_LOCKED(encore_str, take_log, (uintptr_t handle), (handle))
#undef GBP_LOCKED
