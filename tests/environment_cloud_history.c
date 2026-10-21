/* Render cloud history on the GPU: motion must match a fresh frame exactly.
 * The stationary control must use history, so globally disabling it cannot
 * pass. */
#include "../../Vulkan/vulkan_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  size_t target, pipeline, texture, depth;
} Stage;
void *encore_str_from_cstr(const char *s) {
  size_t n = strlen(s);
  encore_str_object *o = malloc(sizeof(*o) + n + 1);
  o->ref_count = 1;
  o->len = n;
  memcpy(o->data, s, n + 1);
  return o;
}
static size_t device, layout, vertex, fragment, indices, uniform, sampler;
static size_t shader(const char *path, unsigned stage) {
  FILE *f = fopen(path, "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  uint32_t *p = malloc(n);
  assert(fread(p, 1, n, f) == (size_t)n);
  fclose(f);
  size_t s = encore_vk_shader_create(device, p, n / 4, stage);
  free(p);
  assert(encore_vk_shader_available(s));
  return s;
}
static Stage create(void) {
  Stage s = {0};
  s.target = encore_vk_render_target_create(device, 96, 64, 97, 1, 97, false);
  assert(encore_vk_swapchain_available(s.target));
  s.pipeline = encore_vk_pipeline_create(
      device, s.target, vertex, fragment, layout, true, false, false, 0, false,
      false, false, 0, NULL, 0, NULL, 0, NULL, 0);
  assert(encore_vk_pipeline_available(s.pipeline));
  s.texture = encore_vk_texture_borrow(
      device, encore_vk_target_attachment(s.target, 0), 96, 64, 97, 5);
  s.depth = encore_vk_texture_borrow(
      device, encore_vk_target_attachment(s.target, 1), 96, 64, 97, 5);
  size_t f = encore_vk_frame_acquire(s.target);
  assert(encore_vk_frame_available(f));
  assert(encore_vk_frame_sampled_color_barrier(f));
  assert(encore_vk_frame_begin_render_pass(f, s.pipeline, 0, 0, 0, 0));
  assert(encore_vk_frame_end_render_pass(f));
  assert(encore_vk_frame_present(f));
  encore_vk_frame_destroy(f);
  assert(encore_vk_swapchain_wait(s.target));
  return s;
}
static void draw(Stage s, Stage history, float *data) {
  void *p = encore_vk_buffer_map(uniform, 576);
  assert(p);
  memcpy(p, data, 576);
  encore_vk_buffer_unmap(uniform);
  uint32_t b[] = {0}, tb[] = {1, 2, 3, 4, 5, 6};
  size_t buffers[] = {uniform},
         textures[] = {history.texture, history.texture, history.texture,
                       history.texture, history.texture, history.depth},
         samplers[] = {sampler, sampler, sampler, sampler, sampler, sampler};
  uint64_t o[] = {0}, r[] = {576};
  size_t g = encore_vk_buffer_textures_bind_group_create_many(
      device, layout, b, 1, buffers, 1, o, 1, r, 1, tb, 6, textures, 6,
      samplers, 6);
  assert(encore_vk_bind_group_available(g));
  size_t f = encore_vk_frame_acquire(s.target);
  assert(encore_vk_frame_available(f));
  assert(encore_vk_frame_sampled_color_barrier(f));
  assert(encore_vk_frame_begin_render_pass(f, s.pipeline, 0, 0, 0, 0));
  assert(encore_vk_frame_bind_group(f, g));
  assert(encore_vk_frame_bind_index_buffer(f, indices));
  assert(encore_vk_frame_draw_indexed(f, 3, 1, 0, 0, 0));
  assert(encore_vk_frame_end_render_pass(f));
  assert(encore_vk_frame_present(f));
  encore_vk_frame_destroy(f);
  assert(encore_vk_swapchain_wait(s.target));
  encore_vk_bind_group_destroy(g);
}
static unsigned char *pixels(Stage s) {
  unsigned n = 96 * 64 * 8;
  unsigned char *p = malloc(n);
  assert(encore_vk_target_readback(s.target, 0, (uintptr_t)p, n) == n);
  return p;
}
static void release(Stage s) {
  encore_vk_pipeline_destroy(s.pipeline);
  encore_vk_texture_destroy(s.texture);
  encore_vk_texture_destroy(s.depth);
  encore_vk_swapchain_destroy(s.target);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  size_t context = encore_vk_create(false);
  assert(encore_vk_available(context));
  device = encore_vk_device_create(context, 0);
  assert(encore_vk_device_available(device));
  vertex = shader("target/generated-shaders/aa_fullscreen.spv", 1);
  fragment = shader(argv[1], 16);
  uint32_t b[] = {0, 1, 2, 3, 4, 5, 6}, t[] = {6, 1, 1, 1, 1, 1, 1},
           st[] = {16, 16, 16, 16, 16, 16, 16};
  layout =
      encore_vk_bind_group_layout_create_entries(device, b, 7, t, 7, st, 7);
  assert(encore_vk_bind_group_layout_available(layout));
  uniform = encore_vk_buffer_create(device, 576, 16, true);
  indices = encore_vk_buffer_create(device, 12, 64, true);
  uint32_t *ip = encore_vk_buffer_map(indices, 12);
  ip[0] = 0;
  ip[1] = 1;
  ip[2] = 2;
  encore_vk_buffer_unmap(indices);
  sampler = encore_vk_sampler_create(device);
  Stage dummy = create(), history = create(), baseline = create(),
        actual = create();
  float data[144] = {0};
  data[0] = 1;
  data[2] = 14;
  data[4] = .8;
  data[8] = 2;
  data[10] = .5;
  data[11] = 18;
  data[15] = 1;
  data[16] = .7;
  data[17] = .7;
  data[18] = .14;
  data[19] = 2;
  data[20] = .65;
  data[21] = 1200;
  data[22] = 600;
  data[23] = 1;
  data[27] = 1;
  data[129] = 4;
  data[133] = .4;
  data[134] = -.916515;
  data[136] = .866025;
  data[141] = .52915;
  data[142] = .23094;
  draw(history, dummy, data);
  encore_vk_texture_destroy(history.texture);
  encore_vk_texture_destroy(history.depth);
  history.texture = encore_vk_texture_borrow(
      device, encore_vk_target_attachment(history.target, 0), 96, 64, 97, 5);
  history.depth = encore_vk_texture_borrow(
      device, encore_vk_target_attachment(history.target, 1), 96, 64, 97, 5);
  memcpy(data + 40, data + 128, 64);
  data[3] = .5;
  data[59] = .5;
  data[23] = .99;
  for (int mode = 0; mode < 4; ++mode) {
    float values[144];
    memcpy(values, data, 576);
    if (mode == 1)
      values[128] += 2;
    if (mode == 2) {
      values[132] = .02;
      values[134] = -.916296;
    }
    if (mode == 3)
      values[136] *= 1.1;
    values[43] = 0;
    draw(baseline, history, values);
    unsigned char *before = pixels(baseline);
    values[43] = 1;
    draw(actual, history, values);
    unsigned char *after = pixels(actual);
    unsigned changed = 0;
    for (unsigned i = 0; i < 96 * 64 * 8; ++i)
      changed += before[i] != after[i];
    printf("mode=%d changed_bytes=%u\n", mode, changed);
    free(before);
    free(after);
    if (mode == 0 && !changed)
      return 3;
    if (mode > 0 && changed)
      return 2;
  }
  release(actual);
  release(baseline);
  release(history);
  release(dummy);
  encore_vk_sampler_destroy(sampler);
  encore_vk_buffer_destroy(indices);
  encore_vk_buffer_destroy(uniform);
  encore_vk_bind_group_layout_destroy(layout);
  encore_vk_shader_destroy(fragment);
  encore_vk_shader_destroy(vertex);
  encore_vk_device_destroy(device);
  encore_vk_destroy(context);
  return 0;
}
