#include "../../native/vfx/vfx.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void tick(GvfxEmitter *e, unsigned n) {
  for (unsigned i = 0; i < n; i++)
    gvfx_step(e, GVFX_FIXED_STEP);
}
static void instruction(float *p, unsigned i, unsigned op, unsigned d,
                        unsigned a, unsigned b, unsigned stage) {
  p[0] = (float)(i + 1);
  float *v = p + 16 + i * 16;
  v[0] = (float)op;
  v[1] = (float)d;
  v[2] = (float)a;
  v[3] = (float)b;
  v[10] = (float)stage;
}
int main(void) {
  GvfxEmitter *a = gvfx_create(128, 42), *b = gvfx_create(128, 42);
  assert(a && b);
  a->rate = b->rate = 120;
  a->lifetime = b->lifetime = .5f;
  tick(a, 120);
  for (unsigned i = 0; i < 60; i++)
    gvfx_step(b, 2 * GVFX_FIXED_STEP);
  assert(a->count == b->count && a->spawned == b->spawned &&
         a->expired == b->expired);
  assert(!memcmp(a->soa, b->soa, 128 * 32 * sizeof(float)));
  a->emitting = false;
  tick(a, 60);
  assert(a->finished && a->count == 0 && a->free_count == 128);
  gvfx_restart(a);
  a->rate = 0;
  a->lifetime = 1;
  assert(gvfx_spawn(a, 200) == 128);
  assert(a->dropped_spawn == 72);
  assert(a->event_count == 128);
  gvfx_emit_event(a, GVFX_USER, 0, 1);
  assert(a->dropped_events == 1);
  a->emitting = false;
  tick(a, 61);
  assert(a->count == 0);
  assert(gvfx_spawn(a, 128) == 128);
  assert(a->next_id == 329);
  gvfx_restart(a);
  float p[16 + 5 * 16 + 16] = {0};
  instruction(p, 0, GVFX_RANDOM, 0, 0, 0, GVFX_SPAWN);
  instruction(p, 1, GVFX_STORE, 1, 0, 0, GVFX_SPAWN);
  p[16 + 16 + 9] = 6;
  instruction(p, 2, GVFX_CONTEXT, 0, 0, 0, GVFX_UPDATE);
  p[16 + 2 * 16 + 4] = 2;
  instruction(p, 3, GVFX_CURVE, 1, 0, 0, GVFX_UPDATE);
  p[16 + 3 * 16 + 4] = 96;
  p[16 + 3 * 16 + 5] = 2;
  instruction(p, 4, GVFX_STORE, 2, 1, 0, GVFX_UPDATE);
  p[16 + 4 * 16 + 9] = 2;
  p[96] = 0;
  p[100] = 1;
  p[101] = 1;
  p[102] = 1;
  p[103] = 1;
  p[104] = 1;
  assert(gvfx_program(a, p, sizeof(p) / sizeof(float)));
  assert(gvfx_spawn(a, 1) == 1);
  float random = a->soa[24 * 128];
  assert(random >= 0 && random < 1);
  tick(a, 30);
  assert(fabsf(a->soa[8 * 128] - .5f) < .001f);
  p[16 + 3 * 16 + 4] = 99999;
  assert(!gvfx_program(a, p, sizeof(p) / sizeof(float)));
  assert(a->program[16 + 3 * 16 + 4] == 96);
  gvfx_restart(a);
  gvfx_spawn(a, 1);
  assert(a->soa[24 * 128] == random);
  a->paused = true;
  float time = a->time;
  gvfx_step(a, 1);
  assert(a->time == time);
  gvfx_destroy(a);
  gvfx_destroy(b);
  GvfxEmitter *wide = gvfx_create(8, 0xdeadbeefu);
  assert(wide);
  wide->next_id = UINT32_MAX - 1;
  wide->generation = 7;
  assert(gvfx_spawn(wide, 4) == 4);
  for (unsigned i = 0; i < 4; i++) {
    unsigned slot = wide->alive[i], stride = wide->capacity;
    uint64_t want = ((uint64_t)7 << 32) + (uint64_t)UINT32_MAX - 1 + i;
    uint64_t got = (uint64_t)wide->soa[19 * stride + slot] |
                   ((uint64_t)wide->soa[21 * stride + slot] << 16) |
                   ((uint64_t)wide->soa[22 * stride + slot] << 32) |
                   ((uint64_t)wide->soa[23 * stride + slot] << 48);
    assert(got == want);
    float *event = wide->events[i].value;
    uint64_t emitted = (uint64_t)event[1] | ((uint64_t)event[13] << 16) |
                       ((uint64_t)event[14] << 32) |
                       ((uint64_t)event[15] << 48);
    assert(emitted == want);
    assert(((uint32_t)event[7] | ((uint32_t)event[11] << 16)) == 0xdeadbeefu);
  }
  gvfx_destroy(wide);
  assert(!gvfx_create(0, 0));
  puts("VFX: determinism, lifecycle, pool reuse, events, graph publication and "
       "curves passed");
}
