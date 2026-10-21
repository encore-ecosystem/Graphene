#define _POSIX_C_SOURCE 200809L
#include "../../../Vulkan/vulkan_api.h"
#include "../../native/vfx/vfx.h"
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
static size_t device, layout, pipeline, target, buffers[6], group, vs, fs, draw,
    indices;
static uint32_t cap;
static float uniform[312];
static size_t shader(const char *path, uint32_t stage) {
  FILE *f = fopen(path, "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  uint32_t *w = malloc(n);
  assert(fread(w, 1, n, f) == (size_t)n);
  fclose(f);
  size_t s = encore_vk_shader_create(device, w, n / 4, stage);
  free(w);
  assert(encore_vk_shader_available(s));
  return s;
}
static void dispatch(size_t frame, uint32_t mode, uint32_t count,
                     uint32_t groups) {
  uniform[48] = (float)mode;
  uniform[49] = (float)count;
  assert(encore_vk_frame_update_f32s(frame, buffers[0], 0, uniform, 312));
  assert(
      encore_vk_frame_dispatch_compute(frame, pipeline, group, groups, 1, 1));
  assert(encore_vk_frame_compute_barrier(frame));
}
static void submit(size_t frame) {
  assert(encore_vk_frame_present(frame));
  encore_vk_frame_destroy(frame);
  assert(encore_vk_swapchain_wait(target));
}
static void create(uint32_t capacity, const float *program, size_t words) {
  cap = capacity;
  uint64_t sizes[6] = {1248,
                       (uint64_t)cap * 128 + 256,
                       (uint64_t)cap * 12 + 32,
                       words * 4,
                       24,
                       262160},
           off[6] = {0};
  uint32_t bindings[6] = {0, 1, 2, 3, 4, 5};
  for (int i = 0; i < 6; i++) {
    buffers[i] = encore_vk_buffer_create(
        device, sizes[i], (i == 0 ? 16 : 32) | 3 | (i == 4 ? 256 : 0), true);
    assert(encore_vk_buffer_available(buffers[i]));
  }
  assert(encore_vk_buffer_write_f32s(buffers[3], 0, program, words));
  group = encore_vk_buffer_bind_group_create_many(device, layout, bindings, 6,
                                                  buffers, 6, off, 6, sizes, 6);
  assert(encore_vk_bind_group_available(group));
  memset(uniform, 0, sizeof uniform);
  for (unsigned i = 0; i < 4; i++)
    uniform[i * 5] = 1;
  uniform[16] = 1;
  uniform[21] = 1;
  uniform[33] = 1;
  uniform[35] = 1;
  for (unsigned i = 36; i < 40; i++)
    uniform[i] = 1;
  uniform[40] = -.5f;
  uniform[41] = .1f;
  uniform[42] = 2;
  uniform[43] = .015f;
  uniform[45] = GVFX_FIXED_STEP;
  uniform[46] = (float)cap;
  uniform[47] = 7;
  uniform[50] = 1;
  size_t frame = encore_vk_frame_acquire(target);
  assert(encore_vk_frame_available(frame));
  assert(encore_vk_frame_sampled_color_barrier(frame));
  dispatch(frame, 0, 0, (cap + 127) / 128);
  submit(frame);
}
static void destroy(void) {
  encore_vk_bind_group_destroy(group);
  for (int i = 0; i < 6; i++)
    encore_vk_buffer_destroy(buffers[i]);
}
static void step(uint32_t spawn, uint32_t first, bool render) {
  size_t f = encore_vk_frame_acquire(target);
  assert(encore_vk_frame_available(f));
  assert(encore_vk_frame_sampled_color_barrier(f));
  assert(encore_vk_frame_fill_buffer(f, buffers[2], 0, 4, 0));
  assert(encore_vk_frame_fill_buffer(f, buffers[5], 0, 4, 0));
  assert(encore_vk_frame_transfer_storage_barrier(f));
  uniform[44] += GVFX_FIXED_STEP;
  uniform[50] = (float)first;
  dispatch(f, 1, 0, (cap + 127) / 128);
  if (spawn) {
    dispatch(f, 3, spawn, 1);
    dispatch(f, 2, spawn, (spawn + 127) / 128);
  }
  dispatch(f, 4, 0, 1);
  if (render) {
    assert(encore_vk_frame_begin_render_pass(f, draw, .01f, .01f, .01f, 1));
    assert(encore_vk_frame_bind_group(f, group));
    assert(encore_vk_frame_bind_index_buffer(f, indices));
    assert(encore_vk_frame_draw_indexed_indirect_count(f, buffers[4], 0,
                                                       buffers[4], 20, 1, 20));
    assert(encore_vk_frame_end_render_pass(f));
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
  size_t s = shader("target/vfx/simulate.comp.spv", 32);
  vs = shader("target/vfx/particle.vert.spv", 1);
  fs = shader("target/vfx/particle.frag.spv", 16);
  uint32_t b[6] = {0, 1, 2, 3, 4, 5}, t[6] = {6, 7, 7, 7, 7, 7},
           st[6] = {33, 33, 33, 32, 32, 32};
  layout =
      encore_vk_bind_group_layout_create_entries(device, b, 6, t, 6, st, 6);
  assert(encore_vk_bind_group_layout_available(layout));
  pipeline = encore_vk_compute_pipeline_create(device, s, layout);
  assert(encore_vk_compute_pipeline_available(pipeline));
  target = encore_vk_offscreen_create(device, 1920, 1080);
  assert(encore_vk_swapchain_available(target));
  draw = encore_vk_pipeline_create(device, target, vs, fs, layout, true, false,
                                   false, 0, true, false, false, 0, NULL, 0,
                                   NULL, 0, NULL, 0);
  assert(encore_vk_pipeline_available(draw));
  indices = encore_vk_buffer_create(device, 24, 64, true);
  uint32_t ix[6] = {0, 1, 2, 2, 1, 3};
  assert(encore_vk_buffer_write_u32s(indices, 0, ix, 6));
  float p[48] = {0};
  p[0] = 2;
  p[16] = GVFX_RANDOM;
  p[17] = 0;
  p[26] = GVFX_SPAWN;
  p[32] = GVFX_STORE;
  p[33] = 1;
  p[41] = 6;
  p[42] = GVFX_SPAWN;
  for (unsigned scenario = 0; scenario < 3; scenario++) {
    if (scenario == 2) {
      p[16] = GVFX_CONST;
      p[41] = 1;
    }
    create(scenario == 0 ? 128 : scenario == 1 ? 16 : 8, p, 48);
    GvfxEmitter *cpu = gvfx_create(cap, 7);
    assert(cpu);
    cpu->rate = 60;
    cpu->gravity = -.5f;
    cpu->drag = .1f;
    cpu->lifetime = 2;
    cpu->size = .015f;
    assert(gvfx_program(cpu, p, 48));
    for (unsigned i = 0; i < (scenario == 1 ? 180u : 60u); i++) {
      gvfx_step(cpu, GVFX_FIXED_STEP);
      step(1, i + 1, false);
    }
    assert(encore_vk_buffer_read_u32(buffers[2], 0) == cpu->count);
    float *gpu = encore_vk_buffer_map(buffers[1], (uint64_t)cap * 128 + 256);
    uint32_t *pool = encore_vk_buffer_map(buffers[2], (uint64_t)cap * 12 + 32);
    assert(gpu && pool);
    float worst = 0;
    for (unsigned i = 0; i < cpu->count; i++) {
      unsigned a = cpu->alive[i], id = (unsigned)cpu->soa[19 * cap + a],
               slot = cap;
      for (unsigned j = 0; j < pool[0]; j++) {
        unsigned candidate = pool[8 + cap + j];
        if ((unsigned)gpu[19 * cap + candidate] == id) {
          slot = candidate;
          break;
        }
      }
      assert(slot < cap);
      for (unsigned c = 0; c < 32; c++) {
        float error = fabsf(cpu->soa[c * cap + a] - gpu[c * cap + slot]);
        worst = fmaxf(worst, error);
        if (error >= .00002f) {
          fprintf(stderr, "id=%u component=%u cpu=%g gpu=%g\n", id, c,
                  cpu->soa[c * cap + a], gpu[c * cap + slot]);
          abort();
        }
      }
    }
    encore_vk_buffer_unmap(buffers[2]);
    encore_vk_buffer_unmap(buffers[1]);
    gvfx_destroy(cpu);
    destroy();
    printf("VFX CPU/GPU scenario %u: max absolute error %.9g\n", scenario,
           worst);
  }
  for (unsigned v = 0; v < 2; v++) {
    unsigned n = v ? 1000000 : 100000;
    float empty[16] = {0};
    create(n, empty, 16);
    uniform[42] = 100;
    step(n, 1, true);
    double start = seconds();
    for (unsigned i = 0; i < 30; i++)
      step(0, 1, true);
    printf("VFX %u coincident alpha-blended sprites, 1920x1080, submit + "
           "simulation + draw + fence: "
           "%.3f ms/frame\n",
           n, (seconds() - start) * 1000 / 30);
    destroy();
  }
  encore_vk_buffer_destroy(indices);
  encore_vk_pipeline_destroy(draw);
  encore_vk_swapchain_destroy(target);
  encore_vk_compute_pipeline_destroy(pipeline);
  encore_vk_shader_destroy(s);
  encore_vk_shader_destroy(vs);
  encore_vk_shader_destroy(fs);
  encore_vk_bind_group_layout_destroy(layout);
  encore_vk_device_destroy(device);
  encore_vk_destroy(instance);
  return 0;
}
