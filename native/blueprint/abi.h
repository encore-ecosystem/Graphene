#ifndef GRAPHENE_BLUEPRINT_ABI_V1_H
#define GRAPHENE_BLUEPRINT_ABI_V1_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBP_ABI_VERSION 3u
#if defined(__aarch64__) || defined(_M_ARM64)
#define GBP_ARCH "aarch64"
#elif defined(__x86_64__) || defined(_M_X64)
#define GBP_ARCH "x86_64"
#else
#error Unsupported Blueprint architecture
#endif
#if defined(_WIN32)
#define GBP_TARGET GBP_ARCH "-windows"
#elif defined(__APPLE__)
#define GBP_TARGET GBP_ARCH "-darwin"
#elif defined(__linux__)
#define GBP_TARGET GBP_ARCH "-linux"
#else
#error Unsupported Blueprint platform
#endif

/* Byte slices are borrowed for the duration of a call, never retained.
 * No Encore values, graph nodes, allocator objects or C++ types cross this ABI. */
typedef struct {
    uint32_t type;
    bool boolean;
    int32_t integer;
    float vector[3];
    uint64_t reference;
    const char *bytes;
    size_t length;
} gbp_value;

typedef struct {
    uint32_t abi;
    uint32_t size;
    void *context;
    bool (*read_transform)(void *, uint64_t, uint32_t, uint32_t, float *);
    bool (*write_transform)(void *, uint64_t, uint32_t, const float *, bool);
    bool (*read_visible)(void *, uint64_t, bool *);
    bool (*write_visible)(void *, uint64_t, bool);
    bool (*print)(void *, const char *, size_t);
    float (*query_scalar)(void *, uint32_t, uint32_t);
    uint64_t (*query_actor)(void *, uint32_t);
    bool (*command)(void *, uint32_t, const float *);
    bool (*event)(void *, const char *, size_t);
    bool (*placement)(void *, const char *, size_t, float);
} gbp_host_v1;

typedef struct {
    uint32_t abi;
    uint32_t size;
    const char *target;
    size_t class_count;
    const char *(*class_id)(size_t, size_t *);
    void *(*create)(size_t);
    void (*destroy)(void *);
    bool (*set_property)(void *, uint64_t, const gbp_value *);
    bool (*get_property)(void *, uint64_t, gbp_value *);
    bool (*invoke)(void *, const gbp_host_v1 *, const char *, size_t, const char *, size_t, float);
    const char *(*error)(void *, uint64_t *, size_t *);
} gbp_api_v1;

typedef const gbp_api_v1 *(*gbp_entry_v1)(void);
#endif
