/* Graphene-specific native integrations. Window/GPU ownership belongs to
 * SDL/gpu; Vulkan resources are supplied by the standalone vulkan refrain. */
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

#include "native/automation/bridge.inc"
#include "native/blueprint/host.inc"

/* Content Browser boundaries: project-relative assets only, no symlink traversal,
 * no replacement on rename, and a bounded binary thumbnail payload. */
#if !defined(_WIN32)
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#endif
extern encore_str encore_str_from_buffer(const void *, size_t);

static char *graphene_content_path(encore_str root, encore_str relative, bool missing_leaf) {
    char *base = graphene_c_string(root), *part = graphene_c_string(relative);
    if (!base || !part) { free(base); free(part); return NULL; }
    size_t a = strlen(base), b = strlen(part);
    bool allowed = !strncmp(part, "Content/", 8) || !strcmp(part, "Content") ||
        !strncmp(part, "game/src/", 9) || !strcmp(part, "game/src") ||
        !strncmp(part, ".graphene/Trash/", 16);
    if (!allowed || !a || !b || a + b + 2 > 8192 || strchr(part, '\\') || strchr(part, ':') ||
        (relative.object && strlen(part) != relative.object->len)) { free(base); free(part); return NULL; }
    char *path = malloc(a + b + 2);
    if (!path) { free(base); free(part); return NULL; }
    memcpy(path, base, a); path[a] = '/'; memcpy(path + a + 1, part, b + 1);
    size_t start = 0;
    for (size_t i = 0; i <= b; ++i) if (part[i] == '/' || part[i] == 0) {
        size_t n = i - start;
        if (!n || (n == 1 && part[start] == '.') || (n == 2 && part[start] == '.' && part[start + 1] == '.')) goto fail;
        char saved = path[a + 1 + i]; path[a + 1 + i] = 0;
#if defined(_WIN32)
        DWORD flags = GetFileAttributesA(path);
        if (flags == INVALID_FILE_ATTRIBUTES) {
            if (!(missing_leaf && i == b)) goto fail;
        } else if (flags & FILE_ATTRIBUTE_REPARSE_POINT) goto fail;
#else
        struct stat st;
        if (lstat(path, &st) != 0) {
            if (!(missing_leaf && i == b && errno == ENOENT)) goto fail;
        } else if (S_ISLNK(st.st_mode)) goto fail;
#endif
        path[a + 1 + i] = saved; start = i + 1;
    }
    free(base); free(part); return path;
fail:
    free(base); free(part); free(path); return NULL;
}
uint64_t graphene_content_stat(encore_str root, encore_str relative) {
    char *path = graphene_content_path(root, relative, false);
    if (!path) return 0;
    uint64_t result = 0;
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA st;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &st)) {
        uint64_t bytes = ((uint64_t)st.nFileSizeHigh << 32) | st.nFileSizeLow;
        result = st.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 2 : (bytes << 2) | 1;
    }
#else
    struct stat st;
    if (lstat(path, &st) == 0) result = S_ISDIR(st.st_mode) ? 2 : S_ISREG(st.st_mode) ? ((uint64_t)st.st_size << 2) | 1 : 0;
#endif
    free(path); return result;
}
bool graphene_content_move(encore_str root, encore_str source, encore_str destination) {
    char *from = graphene_content_path(root, source, false);
    char *to = graphene_content_path(root, destination, true);
    bool ok = false;
    if (from && to && source.object && destination.object &&
        source.object->len > 8 && destination.object->len > 8 &&
        strncmp(source.object->data, "game/", 5) && strncmp(destination.object->data, "game/", 5)) {
#if defined(_WIN32)
        ok = MoveFileA(from, to) != 0;
#elif defined(__linux__) && defined(SYS_renameat2)
        ok = syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, 1u) == 0;
#endif
    }
    free(from); free(to); return ok;
}
encore_str graphene_content_pixels(uintptr_t address, size_t count) {
    if (!address || count < 2 || count > 65538) return graphene_string("");
    return encore_str_from_buffer((const void *)address, count * sizeof(uint32_t));
}

/* Asset containers use explicit little-endian words, independent of host ABI. */
encore_str graphene_asset_bytes(const uint8_t *data, size_t size) {
    if ((!data && size) || size > 268435456) return graphene_string("");
    return encore_str_from_buffer(data, size);
}
encore_str graphene_asset_pixel_bytes(const uint32_t *data, size_t count) {
    if ((!data && count) || count > 67108864) return graphene_string("");
    uint8_t *bytes = malloc(count * 4);
    if (!bytes) return graphene_string("");
    for (size_t i = 0; i < count; ++i) {
        bytes[i*4] = (uint8_t)data[i]; bytes[i*4+1] = (uint8_t)(data[i] >> 8);
        bytes[i*4+2] = (uint8_t)(data[i] >> 16); bytes[i*4+3] = (uint8_t)(data[i] >> 24);
    }
    encore_str result = encore_str_from_buffer(bytes, count * 4);
    free(bytes); return result;
}
encore_str graphene_asset_source_stamp(encore_str input) {
    char *path = graphene_c_string(input);
    if (!path) return graphene_string("");
    char stamp[192]; stamp[0] = 0;
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA st;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &st) && !(st.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        snprintf(stamp, sizeof(stamp), "%lu:%lu:%lu:%lu", (unsigned long)st.nFileSizeHigh, (unsigned long)st.nFileSizeLow,
            (unsigned long)st.ftLastWriteTime.dwHighDateTime, (unsigned long)st.ftLastWriteTime.dwLowDateTime);
#else
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
#if defined(__APPLE__)
        snprintf(stamp, sizeof(stamp), "%llu:%lld:%lld:%ld:%lld:%ld", (unsigned long long)st.st_ino, (long long)st.st_size,
            (long long)st.st_mtimespec.tv_sec, st.st_mtimespec.tv_nsec, (long long)st.st_ctimespec.tv_sec, st.st_ctimespec.tv_nsec);
#else
        snprintf(stamp, sizeof(stamp), "%llu:%lld:%lld:%ld:%lld:%ld", (unsigned long long)st.st_ino, (long long)st.st_size,
            (long long)st.st_mtim.tv_sec, st.st_mtim.tv_nsec, (long long)st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
#endif
    }
#endif
    free(path); return graphene_string(stamp);
}

#include "native/model_import.c"
#include "native/animation_import.c"
#include "native/animation_shaders.c"
#include "native/audio_asset.c"
#include "native/hdr_import.c"

#include "native/smaa_tables.c"

#include "native/aa_shader_data.c"
#include "native/builtin_shader_data.c"
#include "native/environment_shader_data.c"

#include "native/profiling/frame_profiler.inc"

/* Profiler pairs recording workers and queue owner by native frame ID. */

/* Parallel frame completion and rejected-present retirement, revision 2. */

/* Shared GI uploads use owned queue-ordered update packets; page staging is per-slot. */

/* Async timing pairs wait for CPU packet completion; overlapping thread work is not summed. */

#include "native/vfx/bridge.c"
#include "native/json_number.c"

#include "native/mcp/mailbox.inc"
