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
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
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

typedef struct {
    FILE *file;
    void *mapping;
    size_t vertex_bytes;
    size_t triangle_bytes;
    size_t total_bytes;
} graphene_geometry_archive;

/*
 * Runtime-created virtual-geometry archives use a real file mapping. The OS
 * may evict cold source pages while Graphene keeps only its bounded GPU page
 * pool and staging ring resident.
 */
uintptr_t graphene_geometry_archive_create(uintptr_t vertex_data,
    size_t vertex_count, uintptr_t triangle_data, size_t triangle_count) {
    size_t vertex_bytes = vertex_count * sizeof(float);
    size_t triangle_bytes = triangle_count * sizeof(uint32_t);
    if ((vertex_count != 0 && vertex_data == 0) ||
        (triangle_count != 0 && triangle_data == 0) ||
        vertex_bytes > SIZE_MAX - triangle_bytes) return 0;
    size_t total_bytes = vertex_bytes + triangle_bytes;
    if (total_bytes == 0) return 0;
    graphene_geometry_archive *archive =
        (graphene_geometry_archive *)calloc(1, sizeof(*archive));
    if (archive == NULL) return 0;
#if defined(_WIN32)
    archive->mapping = malloc(total_bytes);
    if (archive->mapping == NULL) {
        free(archive);
        return 0;
    }
#else
    archive->file = tmpfile();
    if (archive->file == NULL ||
        ftruncate(fileno(archive->file), (off_t)total_bytes) != 0) {
        if (archive->file != NULL) fclose(archive->file);
        free(archive);
        return 0;
    }
    archive->mapping = mmap(NULL, total_bytes, PROT_READ | PROT_WRITE,
        MAP_SHARED, fileno(archive->file), 0);
    if (archive->mapping == MAP_FAILED) {
        fclose(archive->file);
        free(archive);
        return 0;
    }
#endif
    if (vertex_bytes != 0) {
        memcpy(archive->mapping, (const void *)vertex_data, vertex_bytes);
    }
    if (triangle_bytes != 0) {
        memcpy((unsigned char *)archive->mapping + vertex_bytes,
            (const void *)triangle_data, triangle_bytes);
    }
    archive->vertex_bytes = vertex_bytes;
    archive->triangle_bytes = triangle_bytes;
    archive->total_bytes = total_bytes;
#if !defined(_WIN32)
    (void)msync(archive->mapping, total_bytes, MS_ASYNC);
    (void)madvise(archive->mapping, total_bytes, MADV_RANDOM);
#endif
    return (uintptr_t)archive;
}

bool graphene_geometry_archive_read_vertices(uintptr_t handle,
    size_t first, size_t count, uintptr_t destination) {
    graphene_geometry_archive *archive =
        (graphene_geometry_archive *)handle;
    if (archive == NULL || destination == 0 ||
        first > archive->vertex_bytes / sizeof(float) ||
        count > archive->vertex_bytes / sizeof(float) - first) return false;
    memcpy((void *)destination,
        (const float *)archive->mapping + first, count * sizeof(float));
    return true;
}

bool graphene_geometry_archive_read_triangles(uintptr_t handle,
    size_t first, size_t count, uintptr_t destination) {
    graphene_geometry_archive *archive =
        (graphene_geometry_archive *)handle;
    if (archive == NULL || destination == 0 ||
        first > archive->triangle_bytes / sizeof(uint32_t) ||
        count > archive->triangle_bytes / sizeof(uint32_t) - first) return false;
    memcpy((void *)destination,
        (const uint32_t *)((const unsigned char *)archive->mapping +
            archive->vertex_bytes) + first,
        count * sizeof(uint32_t));
    return true;
}

bool graphene_geometry_archive_prefetch(uintptr_t handle,
    size_t vertex_first, size_t vertex_count,
    size_t triangle_first, size_t triangle_count) {
    graphene_geometry_archive *archive =
        (graphene_geometry_archive *)handle;
    if (archive == NULL ||
        vertex_first > archive->vertex_bytes / sizeof(float) ||
        vertex_count > archive->vertex_bytes / sizeof(float) - vertex_first ||
        triangle_first > archive->triangle_bytes / sizeof(uint32_t) ||
        triangle_count > archive->triangle_bytes / sizeof(uint32_t) -
            triangle_first) return false;
#if !defined(_WIN32) && defined(POSIX_FADV_WILLNEED)
    int fd = fileno(archive->file);
    off_t vertex_offset = (off_t)(vertex_first * sizeof(float));
    off_t triangle_offset = (off_t)(archive->vertex_bytes +
        triangle_first * sizeof(uint32_t));
    int vertex_result = posix_fadvise(fd, vertex_offset,
        (off_t)(vertex_count * sizeof(float)), POSIX_FADV_WILLNEED);
    int triangle_result = posix_fadvise(fd, triangle_offset,
        (off_t)(triangle_count * sizeof(uint32_t)), POSIX_FADV_WILLNEED);
    return vertex_result == 0 && triangle_result == 0;
#else
    return true;
#endif
}

size_t graphene_geometry_archive_bytes(uintptr_t handle) {
    graphene_geometry_archive *archive =
        (graphene_geometry_archive *)handle;
    return archive == NULL ? 0 : archive->total_bytes;
}

bool graphene_geometry_archive_destroy(uintptr_t handle) {
    graphene_geometry_archive *archive =
        (graphene_geometry_archive *)handle;
    if (archive == NULL) return true;
#if defined(_WIN32)
    free(archive->mapping);
#else
    if (archive->mapping != NULL) {
        (void)munmap(archive->mapping, archive->total_bytes);
    }
    if (archive->file != NULL) fclose(archive->file);
#endif
    free(archive);
    return true;
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

/* Deterministic scalar kernel used by the Encore rigid-body facade. Keeping
 * the integration and restitution math in the Graphene native runtime gives
 * us a stable ABI seam for a future SIMD/broadphase solver without changing
 * gameplay scripts. */
float graphene_physics_floor_position(float position, float velocity,
    float gravity, float delta_seconds, float radius) {
    float next = position + (velocity + gravity * delta_seconds) * delta_seconds;
    return next < radius ? radius : next;
}

float graphene_physics_floor_velocity(float position, float velocity,
    float gravity, float delta_seconds, float radius, float restitution) {
    float integrated = position + (velocity + gravity * delta_seconds) * delta_seconds;
    if (integrated < radius && velocity + gravity * delta_seconds < 0.0f) {
        float bounce = restitution < 0.0f ? 0.0f : restitution;
        return -(velocity + gravity * delta_seconds) * bounce;
    }
    return velocity + gravity * delta_seconds;
}

#include "workspace/automation_native.inc"
