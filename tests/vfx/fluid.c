#include "../../native/vfx/fluid.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double mass(GvfxFluid *f) {
  double s = 0;
  for (unsigned i = 0; i < f->cells; i++)
    s += f->density[i];
  return s;
}
int main(void) {
  for (unsigned depth = 1; depth <= 16; depth += 15) {
    GvfxFluid *f = gvfx_fluid_create(16, 16, depth, 0, .25f, 0);
    assert(f);
    float p[3] = {2, 1, depth == 1 ? 0 : 2}, v[3] = {.1f, 1, .2f};
    gvfx_fluid_source(f, p, .7f, 4, 3, 2, v, .1f);
    assert(mass(f) > 0);
    for (unsigned i = 0; i < 20; i++)
      gvfx_fluid_step(f, 1.f / 60);
    assert(isfinite(f->divergence_after));
    assert(f->divergence_after < f->divergence_before);
    for (unsigned i = 0; i < f->cells; i++)
      assert(isfinite(f->density[i]) && f->density[i] >= 0);
    gvfx_fluid_destroy(f);
  }
  GvfxFluid *w = gvfx_fluid_create(32, 32, 1, 2, .25f, 0);
  assert(w);
  for (unsigned y = 0; y < 32; y++)
    for (unsigned x = 0; x < 32; x++)
      w->density[y * 32 + x] =
          1 + .2f * expf(-(((float)x - 16) * ((float)x - 16) +
                           ((float)y - 16) * ((float)y - 16)) *
                         .02f);
  double before = mass(w);
  for (unsigned i = 0; i < 120; i++)
    gvfx_fluid_step(w, 1.f / 60);
  assert(fabs(mass(w) - before) < before * .00001);
  gvfx_fluid_destroy(w);
  GvfxFluid *l = gvfx_fluid_create(16, 16, 16, 1, .25f, 4096);
  assert(l);
  float p[3] = {2, 2, 2}, v[3] = {0, 0, 0};
  gvfx_fluid_source(l, p, .7f, 1, 0, 0, v, .1f);
  assert(l->particle_count > 0);
  for (unsigned i = 0; i < 30; i++)
    gvfx_fluid_step(l, 1.f / 60);
  for (unsigned i = 0; i < l->particle_count; i++)
    for (unsigned a = 0; a < 3; a++)
      assert(isfinite(l->particles[i].position[a]) &&
             l->particles[i].position[a] >= 0 &&
             l->particles[i].position[a] <= 4);
  gvfx_fluid_destroy(l);
  puts("VFX fluids: 2D/3D gas projection, finite fields, conservative shallow "
       "water and PIC/FLIP passed");
}
