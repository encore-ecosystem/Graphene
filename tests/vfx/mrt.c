/* A translucent pass must replace the auxiliary velocity/depth/validity data.
 */
#include "../../../Vulkan/vulkan_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void *encore_str_from_cstr(const char *s) {
  size_t n = strlen(s);
  encore_str_object *o = malloc(sizeof *o + n + 1);
  assert(o);
  o->ref_count = 1;
  o->len = n;
  memcpy(o->data, s, n + 1);
  return o;
}
static size_t shader(size_t device, const char *path, unsigned stage) {
  FILE *file = fopen(path, "rb");
  assert(file);
  fseek(file, 0, SEEK_END);
  long bytes = ftell(file);
  rewind(file);
  uint32_t *words = malloc(bytes);
  assert(words);
  assert(fread(words, 1, bytes, file) == (size_t)bytes);
  fclose(file);
  size_t handle = encore_vk_shader_create(device, words, bytes / 4, stage);
  free(words);
  assert(encore_vk_shader_available(handle));
  return handle;
}
int main(void) {
  size_t instance = encore_vk_create(false),
         device = encore_vk_device_create(instance, 0);
  assert(encore_vk_device_available(device));
  if (!encore_vk_device_independent_blend(device)) {
    puts("MRT data-write test skipped: independentBlend unavailable");
    encore_vk_device_destroy(device);
    encore_vk_destroy(instance);
    return 0;
  }
  size_t target =
      encore_vk_render_target_create(device, 2, 2, 37, 1, 97, false);
  assert(encore_vk_swapchain_available(target));
  size_t vertex = shader(device, "target/vfx/fullscreen.vert.spv", 1),
         opaque_fragment = shader(device, "target/vfx/mrt-opaque.spv", 16),
         translucent_fragment =
             shader(device, "target/vfx/mrt-translucent.spv", 16);
  size_t opaque = encore_vk_pipeline_create(
      device, target, vertex, opaque_fragment, 0, true, false, false, 0, false,
      false, false, 0, NULL, 0, NULL, 0, NULL, 0);
  size_t translucent = encore_vk_pipeline_create(
      device, target, vertex, translucent_fragment, 0, true, false, false, 0,
      true, false, false, 0, NULL, 0, NULL, 0, NULL, 0);
  assert(encore_vk_pipeline_available(opaque) &&
         encore_vk_pipeline_available(translucent));
  size_t indices = encore_vk_buffer_create(device, 12, 64, true);
  uint32_t ix[3] = {0, 1, 2};
  assert(encore_vk_buffer_write_u32s(indices, 0, ix, 3));
  size_t frame = encore_vk_frame_acquire(target);
  assert(encore_vk_frame_begin_render_pass(frame, opaque, 0, 0, 0, 1));
  assert(encore_vk_frame_bind_index_buffer(frame, indices));
  assert(encore_vk_frame_draw_indexed(frame, 3, 1, 0, 0, 0));
  assert(encore_vk_frame_bind_pipeline(frame, translucent));
  assert(encore_vk_frame_draw_indexed(frame, 3, 1, 0, 0, 0));
  assert(encore_vk_frame_end_render_pass(frame));
  assert(encore_vk_frame_present(frame));
  encore_vk_frame_destroy(frame);
  uint16_t data[16];
  assert(encore_vk_target_readback(target, 1, (uintptr_t)data, sizeof data) ==
         sizeof data);
  for (unsigned i = 0; i < 4; i++) {
    assert(data[4 * i] == 0x3400);
    assert(data[4 * i + 1] == 0x3800);
    assert(data[4 * i + 2] == 0x4500);
    assert(data[4 * i + 3] == 0);
  }
  encore_vk_buffer_destroy(indices);
  encore_vk_pipeline_destroy(opaque);
  encore_vk_pipeline_destroy(translucent);
  encore_vk_swapchain_destroy(target);
  encore_vk_shader_destroy(vertex);
  encore_vk_shader_destroy(opaque_fragment);
  encore_vk_shader_destroy(translucent_fragment);
  encore_vk_device_destroy(device);
  encore_vk_destroy(instance);
  puts("MRT translucency: unblended motion/depth/reactive data passed");
  return 0;
}
