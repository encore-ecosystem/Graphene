#define _POSIX_C_SOURCE 200809L
#include "../../../Vulkan/vulkan_api.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
void *encore_str_from_cstr(const char *s) {
  size_t n = strlen(s);
  encore_str_object *o = malloc(sizeof *o + n + 1);
  assert(o);
  o->ref_count = 1;
  o->len = n;
  memcpy(o->data, s, n + 1);
  return o;
}
static size_t device, layout, pipeline, target, buffers[7], group;
static unsigned n, capacity;
static float uniform[84];
static size_t shader(const char *path) {
  FILE *f = fopen(path, "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  uint32_t *w = malloc(n);
  assert(fread(w, 1, n, f) == (size_t)n);
  fclose(f);
  size_t s = encore_vk_shader_create(device, w, n / 4, 32);
  free(w);
  assert(encore_vk_shader_available(s));
  return s;
}
static void dispatch(size_t frame, unsigned mode, unsigned count,
                     unsigned parity) {
  uniform[46] = (float)mode;
  uniform[47] = (float)parity;
  assert(encore_vk_frame_update_f32s(frame, buffers[0], 0, uniform, 84));
  assert(encore_vk_frame_dispatch_compute(frame, pipeline, group,
                                          (count + 127) / 128, 1, 1));
  assert(encore_vk_frame_compute_barrier(frame));
}
static void submit(size_t frame) {
  assert(encore_vk_frame_present(frame));
  encore_vk_frame_destroy(frame);
  assert(encore_vk_swapchain_wait(target));
}
static void create(unsigned x, unsigned y, unsigned z, unsigned kind) {
  n = x * y * z;
  capacity = kind == 1 ? 8192 : 0;
  uint64_t sizes[7] = {336,
                       (uint64_t)n * 32,
                       (uint64_t)n * 32,
                       (uint64_t)n * 8,
                       (uint64_t)n * 4,
                       16 + (uint64_t)capacity * 32,
                       (uint64_t)(n + capacity) * 4},
           off[7] = {0};
  uint32_t bindings[7] = {0, 1, 2, 3, 4, 5, 6};
  for (unsigned i = 0; i < 7; i++) {
    buffers[i] =
        encore_vk_buffer_create(device, sizes[i], (i == 0 ? 16 : 32) | 3, true);
    assert(encore_vk_buffer_available(buffers[i]));
  }
  group = encore_vk_buffer_bind_group_create_many(device, layout, bindings, 7,
                                                  buffers, 7, off, 7, sizes, 7);
  assert(encore_vk_bind_group_available(group));
  memset(uniform, 0, sizeof uniform);
  uniform[36] = x * .1f;
  uniform[37] = y * .1f;
  uniform[38] = z * .1f;
  uniform[40] = (float)x;
  uniform[41] = (float)y;
  uniform[42] = (float)z;
  uniform[43] = .1f;
  uniform[44] = 1.f / 60;
  uniform[48] = uniform[36] * .5f;
  uniform[49] = uniform[37] * .25f;
  uniform[50] = uniform[38] * .5f;
  uniform[51] = .4f;
  uniform[52] = 4;
  uniform[53] = 3;
  uniform[54] = 1;
  uniform[55] = .1f;
  uniform[57] = 1;
  uniform[59] = 1;
  uniform[60] = -9.81f;
  uniform[61] = 2;
  uniform[62] = 40;
  uniform[63] = (float)kind;
  uniform[80] = (float)capacity;
  uniform[81] = 7;
  uniform[82] = .95f;
  uniform[83] = .5f;
  size_t f = encore_vk_frame_acquire(target);
  assert(encore_vk_frame_available(f));
  dispatch(f, 0, n, 0);
  if (kind == 1)
    dispatch(f, 11, capacity, 0);
  submit(f);
}
static void destroy(void) {
  encore_vk_bind_group_destroy(group);
  for (unsigned i = 0; i < 7; i++)
    encore_vk_buffer_destroy(buffers[i]);
}
static void step(unsigned kind) {
  size_t f = encore_vk_frame_acquire(target);
  assert(encore_vk_frame_available(f));
  uniform[45] += uniform[44];
  if (kind == 2) {
    dispatch(f, 12, n, 0);
    dispatch(f, 6, n, 0);
  } else {
    if (kind == 1) {
      dispatch(f, 7, n, 0);
      dispatch(f, 8, capacity, 0);
      dispatch(f, 9, n, 0);
    } else
      dispatch(f, 1, n, 0);
    dispatch(f, 2, n, 0);
    dispatch(f, 3, n, 0);
    for (unsigned i = 0; i < 40; i++)
      dispatch(f, 4, n, i % 2);
    dispatch(f, 5, n, 0);
    if (kind == 1)
      dispatch(f, 10, capacity, 0);
  }
  submit(f);
}
static double seconds(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}
int main(void) {
  size_t instance = encore_vk_create(false);
  assert(encore_vk_available(instance));
  device = encore_vk_device_create(instance, 0);
  assert(encore_vk_device_available(device));
  size_t s = shader("target/vfx/fluid.comp.spv");
  uint32_t b[7] = {0, 1, 2, 3, 4, 5, 6}, t[7] = {6, 7, 7, 7, 7, 7, 7},
           st[7] = {32, 32, 32, 32, 32, 32, 32};
  layout =
      encore_vk_bind_group_layout_create_entries(device, b, 7, t, 7, st, 7);
  assert(encore_vk_bind_group_layout_available(layout));
  pipeline = encore_vk_compute_pipeline_create(device, s, layout);
  assert(encore_vk_compute_pipeline_available(pipeline));
  target = encore_vk_offscreen_create(device, 8, 8);
  assert(encore_vk_swapchain_available(target));
  for (unsigned kind = 0; kind < 3; kind++) {
    create(32, 32, kind == 2 ? 1 : 32, kind);
    double start = seconds();
    for (unsigned i = 0; i < 30; i++)
      step(kind);
    double elapsed = seconds() - start;
    float *data = encore_vk_buffer_map(buffers[1], (uint64_t)n * 32);
    assert(data);
    double mass = 0;
    for (unsigned i = 0; i < n; i++) {
      for (unsigned j = 0; j < 8; j++)
        assert(isfinite(data[i * 8 + j]));
      assert(data[i * 8 + 3] >= 0);
      mass += data[i * 8 + 3];
    }
    if (kind == 2)
      assert(fabs(mass - 512) < .001);
    else
      assert(mass > 0);
    encore_vk_buffer_unmap(buffers[1]);
    if (kind == 1) {
      float *p = encore_vk_buffer_map(buffers[5], 16 + (uint64_t)capacity * 32);
      assert(p);
      for (unsigned i = 0; i < capacity; i++)
        for (unsigned a = 0; a < 3; a++)
          assert(isfinite(p[4 + i * 8 + a]) && p[4 + i * 8 + a] >= 0 &&
                 p[4 + i * 8 + a] <= 3.2f);
      encore_vk_buffer_unmap(buffers[5]);
    }
    printf("GPU fluid kind %u: finite fields, mass %.6f, %.3f ms/step "
           "including submission and fence\n",
           kind, mass, elapsed * 1000 / 30);
    destroy();
  }
  for (unsigned resolution = 64; resolution <= 128; resolution *= 2) {
    create(resolution, resolution, resolution, 0);
    double start = seconds();
    for (unsigned i = 0; i < 4; i++)
      step(0);
    printf("GPU gas %u^3, 40 Jacobi iterations: %.3f ms/step including "
           "submission and fence\n",
           resolution, (seconds() - start) * 1000 / 4);
    destroy();
  }
  encore_vk_swapchain_destroy(target);
  encore_vk_compute_pipeline_destroy(pipeline);
  encore_vk_shader_destroy(s);
  encore_vk_bind_group_layout_destroy(layout);
  encore_vk_device_destroy(device);
  encore_vk_destroy(instance);
  return 0;
}
