#define main vfx_full_test_main
#include "gpu.c"
#undef main
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

  size_t source = encore_vk_buffer_create(device, 64, 3, false);
  size_t packets[2] = {encore_vk_buffer_create(device, 64, 3, true),
                       encore_vk_buffer_create(device, 64, 3, true)};
  unsigned expected[2] = {0}, observed = 0;
  for (unsigned sequence = 1; sequence <= 50; sequence++) {
    size_t f = encore_vk_frame_acquire(target);
    assert(encore_vk_frame_available(f));
    unsigned slot = encore_vk_frame_slot(f);
    assert(slot < 2);
    if (expected[slot]) {
      float sample[16];
      assert(encore_vk_buffer_read_f32s(packets[slot], 0, sample, 16));
      for (unsigned i = 0; i < 16; i++)
        assert(sample[i] == (float)expected[slot]);
      observed++;
    }
    float sample[16];
    for (unsigned i = 0; i < 16; i++)
      sample[i] = (float)sequence;
    assert(encore_vk_frame_update_f32s(f, source, 0, sample, 16));
    assert(encore_vk_frame_copy_buffer(f, source, packets[slot], 0, 0, 64));
    assert(encore_vk_frame_begin_render_pass(f, draw, 0, 0, 0, 1));
    assert(encore_vk_frame_end_render_pass(f));
    assert(encore_vk_frame_present(f));
    encore_vk_frame_destroy(f);
    expected[slot] = sequence;
  }
  assert(observed >= 48);
  assert(encore_vk_swapchain_wait(target));
  assert(!encore_vk_buffer_read_f32s(source, 0, NULL, 1));
  assert(!encore_vk_buffer_read_f32s(packets[0], 64, (float[1]){0}, 1));
  assert(!encore_vk_buffer_read_f32s(packets[0], 0, (float[1]){0}, SIZE_MAX));
  assert(encore_vk_buffer_read_f32s(packets[0], 64, NULL, 0));
  for (unsigned i = 0; i < 2; i++)
    encore_vk_buffer_destroy(packets[i]);
  encore_vk_buffer_destroy(source);
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
  printf("Asynchronous slot readback: %u completed packets; no explicit "
         "per-frame wait\n",
         observed);
  return 0;
}
