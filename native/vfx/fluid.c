/* Original position-based fluid simulation used by the VFX runtime. */
#include "fluid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static uint32_t gvfx_cell(GvfxFluid *f, int x, int y, int z) {
  x = x < 0 ? 0 : x >= (int)f->nx ? (int)f->nx - 1 : x;
  y = y < 0 ? 0 : y >= (int)f->ny ? (int)f->ny - 1 : y;
  z = z < 0 ? 0 : z >= (int)f->nz ? (int)f->nz - 1 : z;
  return ((uint32_t)z * f->ny + (uint32_t)y) * f->nx + (uint32_t)x;
}
GvfxFluid *gvfx_fluid_create(uint32_t nx, uint32_t ny, uint32_t nz,
                             uint32_t kind, float h, uint32_t particles) {
  if (nx < 4 || ny < 4 || !nz || nx > 512 || ny > 512 || nz > 512 ||
      (uint64_t)nx * ny * nz > 16777216 || kind > 2 || !isfinite(h) || h <= 0 ||
      particles > 4194304)
    return NULL;
  GvfxFluid *f = calloc(1, sizeof *f);
  if (!f)
    return NULL;
  f->nx = nx;
  f->ny = ny;
  f->nz = nz;
  f->cells = nx * ny * nz;
  f->kind = kind;
  f->cell_size = h;
  f->iterations = 40;
  f->flip = .95f;
  f->gravity = -9.81f;
  f->buoyancy = 1;
  f->burn_rate = 2;
  f->dissipation = .1f;
  f->particle_capacity = particles;
  size_t n = f->cells;
  f->velocity = calloc(n * 3, sizeof(float));
  f->previous = calloc(n * 3, sizeof(float));
  f->scratch = calloc(n * 3, sizeof(float));
  f->density = calloc(n, sizeof(float));
  f->temperature = calloc(n, sizeof(float));
  f->fuel = calloc(n, sizeof(float));
  f->pressure = calloc(n, sizeof(float));
  f->pressure_next = calloc(n, sizeof(float));
  f->divergence = calloc(n, sizeof(float));
  f->weights = calloc(n, sizeof(float));
  f->obstacle_velocity = calloc(n * 3, sizeof(float));
  f->solid = calloc(n, 1);
  f->liquid = calloc(n, 1);
  f->particles =
      particles ? calloc(particles, sizeof(GvfxLiquidParticle)) : NULL;
  if (!f->velocity || !f->previous || !f->scratch || !f->density ||
      !f->temperature || !f->fuel || !f->pressure || !f->pressure_next ||
      !f->divergence || !f->weights || !f->obstacle_velocity || !f->solid ||
      !f->liquid || (particles && !f->particles)) {
    gvfx_fluid_destroy(f);
    return NULL;
  }
  return f;
}
void gvfx_fluid_destroy(GvfxFluid *f) {
  if (!f)
    return;
  free(f->velocity);
  free(f->previous);
  free(f->scratch);
  free(f->density);
  free(f->temperature);
  free(f->fuel);
  free(f->pressure);
  free(f->pressure_next);
  free(f->divergence);
  free(f->weights);
  free(f->obstacle_velocity);
  free(f->solid);
  free(f->liquid);
  free(f->particles);
  free(f);
}
static float gvfx_fluid_sample(GvfxFluid *f, const float *data, float x,
                               float y, float z, uint32_t stride, uint32_t c) {
  x = fminf(f->nx - 1, fmaxf(0, x));
  y = fminf(f->ny - 1, fmaxf(0, y));
  z = fminf(f->nz - 1, fmaxf(0, z));
  int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
  float fx = x - ix, fy = y - iy, fz = z - iz, out = 0;
  for (int k = 0; k < 2; k++)
    for (int j = 0; j < 2; j++)
      for (int i = 0; i < 2; i++)
        out += data[(size_t)gvfx_cell(f, ix + i, iy + j, iz + k) * stride + c] *
               (i ? fx : 1 - fx) * (j ? fy : 1 - fy) * (k ? fz : 1 - fz);
  return out;
}
static void gvfx_fluid_advect(GvfxFluid *f, float *field, uint32_t stride,
                              float dt) {
  for (uint32_t z = 0; z < f->nz; z++)
    for (uint32_t y = 0; y < f->ny; y++)
      for (uint32_t x = 0; x < f->nx; x++) {
        uint32_t i = gvfx_cell(f, x, y, z);
        float bx = x - f->velocity[i * 3] * dt / f->cell_size,
              by = y - f->velocity[i * 3 + 1] * dt / f->cell_size,
              bz = z - f->velocity[i * 3 + 2] * dt / f->cell_size;
        for (uint32_t c = 0; c < stride; c++)
          f->scratch[i * stride + c] =
              f->solid[i] ? 0
                          : gvfx_fluid_sample(f, field, bx, by, bz, stride, c);
      }
  memcpy(field, f->scratch, (size_t)f->cells * stride * sizeof(float));
}
static uint32_t gvfx_neighbour(GvfxFluid *f, uint32_t x, uint32_t y, uint32_t z,
                               int axis, int direction) {
  return gvfx_cell(f, (int)x + (axis == 0 ? direction : 0),
                   (int)y + (axis == 1 ? direction : 0),
                   (int)z + (axis == 2 ? direction : 0));
}
static float gvfx_fluid_divergence(GvfxFluid *f) {
  double sum = 0;
  uint32_t count = 0;
  for (uint32_t z = 0; z < f->nz; z++)
    for (uint32_t y = 0; y < f->ny; y++)
      for (uint32_t x = 0; x < f->nx; x++) {
        uint32_t i = gvfx_cell(f, x, y, z);
        if (f->solid[i]) {
          f->divergence[i] = 0;
          continue;
        }
        float d = 0;
        for (int a = 0; a < (f->nz == 1 ? 2 : 3); a++) {
          uint32_t l = gvfx_neighbour(f, x, y, z, a, -1),
                   r = gvfx_neighbour(f, x, y, z, a, 1);
          float vl = f->solid[l] ? f->obstacle_velocity[l * 3 + a]
                                 : f->velocity[l * 3 + a],
                vr = f->solid[r] ? f->obstacle_velocity[r * 3 + a]
                                 : f->velocity[r * 3 + a];
          d += (vr - vl) / (2 * f->cell_size);
        }
        f->divergence[i] = d;
        sum += d * d;
        count++;
      }
  return count ? sqrtf((float)(sum / count)) : 0;
}
void gvfx_fluid_project(GvfxFluid *f, float dt) {
  if (!f || dt <= 0)
    return;
  f->divergence_before = gvfx_fluid_divergence(f);
  memset(f->pressure, 0, (size_t)f->cells * sizeof(float));
  for (uint32_t iteration = 0; iteration < f->iterations; iteration++) {
    for (uint32_t z = 0; z < f->nz; z++)
      for (uint32_t y = 0; y < f->ny; y++)
        for (uint32_t x = 0; x < f->nx; x++) {
          uint32_t i = gvfx_cell(f, x, y, z);
          if (f->solid[i] || (f->kind == 1 && !f->liquid[i])) {
            f->pressure_next[i] = 0;
            continue;
          }
          float sum = 0, n = 0;
          for (int a = 0; a < (f->nz == 1 ? 2 : 3); a++)
            for (int sign = -1; sign <= 1; sign += 2) {
              uint32_t j = gvfx_neighbour(f, x, y, z, a, sign);
              if (j != i && !f->solid[j]) {
                sum += f->pressure[j];
                n++;
              }
            }
          f->pressure_next[i] = n > 0 ? (sum - f->divergence[i] * f->cell_size *
                                                   f->cell_size / dt) /
                                            n
                                      : 0;
        }
    float *swap = f->pressure;
    f->pressure = f->pressure_next;
    f->pressure_next = swap;
  }
  for (uint32_t z = 0; z < f->nz; z++)
    for (uint32_t y = 0; y < f->ny; y++)
      for (uint32_t x = 0; x < f->nx; x++) {
        uint32_t i = gvfx_cell(f, x, y, z);
        for (int a = 0; a < (f->nz == 1 ? 2 : 3); a++) {
          if (f->solid[i]) {
            f->velocity[i * 3 + a] = f->obstacle_velocity[i * 3 + a];
            continue;
          }
          uint32_t l = gvfx_neighbour(f, x, y, z, a, -1),
                   r = gvfx_neighbour(f, x, y, z, a, 1);
          float pl = f->solid[l] ? f->pressure[i] : f->pressure[l],
                pr = f->solid[r] ? f->pressure[i] : f->pressure[r];
          f->velocity[i * 3 + a] -= dt * (pr - pl) / (2 * f->cell_size);
          if (l == i || r == i || f->solid[l] || f->solid[r])
            f->velocity[i * 3 + a] =
                f->solid[l]   ? f->obstacle_velocity[l * 3 + a]
                : f->solid[r] ? f->obstacle_velocity[r * 3 + a]
                              : 0;
        }
      }
  f->divergence_after = gvfx_fluid_divergence(f);
}
void gvfx_fluid_obstacle(GvfxFluid *f, const float p[3], float radius,
                         const float velocity[3]) {
  if (!f || !p || !velocity || !isfinite(radius) || radius < 0)
    return;
  for (uint32_t z = 0; z < f->nz; z++)
    for (uint32_t y = 0; y < f->ny; y++)
      for (uint32_t x = 0; x < f->nx; x++) {
        uint32_t i = gvfx_cell(f, x, y, z);
        float dx = (x + .5f) * f->cell_size - p[0],
              dy = (y + .5f) * f->cell_size - p[1],
              dz = f->nz == 1 ? 0 : (z + .5f) * f->cell_size - p[2];
        f->solid[i] = dx * dx + dy * dy + dz * dz < radius * radius;
        for (int a = 0; a < 3; a++)
          f->obstacle_velocity[i * 3 + a] = velocity[a];
      }
}
void gvfx_fluid_source(GvfxFluid *f, const float p[3], float radius,
                       float density, float heat, float fuel,
                       const float velocity[3], float dt) {
  if (!f || !p || !velocity || !isfinite(radius) || radius <= 0 ||
      !isfinite(dt) || dt <= 0)
    return;
  for (uint32_t z = 0; z < f->nz; z++)
    for (uint32_t y = 0; y < f->ny; y++)
      for (uint32_t x = 0; x < f->nx; x++) {
        uint32_t i = gvfx_cell(f, x, y, z);
        float dx = (x + .5f) * f->cell_size - p[0],
              dy = (y + .5f) * f->cell_size - p[1],
              dz = f->nz == 1 ? 0 : (z + .5f) * f->cell_size - p[2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > radius || f->solid[i])
          continue;
        float weight = (1 - d / radius) * dt;
        f->density[i] = fmaxf(0, f->density[i] + density * weight);
        f->temperature[i] = fmaxf(0, f->temperature[i] + heat * weight);
        f->fuel[i] = fmaxf(0, f->fuel[i] + fuel * weight);
        for (int a = 0; a < 3; a++)
          f->velocity[i * 3 + a] += velocity[a] * weight;
        if (f->kind == 1 && f->particle_count < f->particle_capacity) {
          GvfxLiquidParticle *q = &f->particles[f->particle_count++];
          q->position[0] = (x + .5f) * f->cell_size;
          q->position[1] = (y + .5f) * f->cell_size;
          q->position[2] = (z + .5f) * f->cell_size;
          memcpy(q->velocity, velocity, 3 * sizeof(float));
        }
      }
}
static void gvfx_liquid_to_grid(GvfxFluid *f) {
  memset(f->velocity, 0, (size_t)f->cells * 3 * sizeof(float));
  memset(f->weights, 0, (size_t)f->cells * sizeof(float));
  memset(f->liquid, 0, f->cells);
  for (uint32_t i = 0; i < f->particle_count; i++) {
    GvfxLiquidParticle *p = &f->particles[i];
    float x = p->position[0] / f->cell_size - .5f,
          y = p->position[1] / f->cell_size - .5f,
          z = p->position[2] / f->cell_size - .5f;
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    for (int k = 0; k < 2; k++)
      for (int j = 0; j < 2; j++)
        for (int a = 0; a < 2; a++) {
          uint32_t cell = gvfx_cell(f, ix + a, iy + j, iz + k);
          float w = (a ? fx : 1 - fx) * (j ? fy : 1 - fy) * (k ? fz : 1 - fz);
          f->weights[cell] += w;
          if (w > .001f)
            f->liquid[cell] = 1;
          for (int c = 0; c < 3; c++)
            f->velocity[cell * 3 + c] += p->velocity[c] * w;
        }
  }
  for (uint32_t i = 0; i < f->cells; i++) {
    if (f->weights[i] > 1e-6f)
      for (int a = 0; a < 3; a++)
        f->velocity[i * 3 + a] /= f->weights[i];
    f->density[i] = fminf(1, f->weights[i]);
  }
  memcpy(f->previous, f->velocity, (size_t)f->cells * 3 * sizeof(float));
}
static void gvfx_liquid_from_grid(GvfxFluid *f, float dt) {
  for (uint32_t i = 0; i < f->particle_count; i++) {
    GvfxLiquidParticle *p = &f->particles[i];
    float q[3];
    for (int a = 0; a < 3; a++)
      q[a] = p->position[a] / f->cell_size - .5f;
    for (int a = 0; a < 3; a++) {
      float pic = gvfx_fluid_sample(f, f->velocity, q[0], q[1], q[2], 3, a),
            old = gvfx_fluid_sample(f, f->previous, q[0], q[1], q[2], 3, a);
      p->velocity[a] =
          (1 - f->flip) * pic + f->flip * (p->velocity[a] + pic - old);
      p->position[a] += p->velocity[a] * dt;
      float limit = (a == 0 ? f->nx : a == 1 ? f->ny : f->nz) * f->cell_size;
      if (p->position[a] < .1f * f->cell_size) {
        p->position[a] = .1f * f->cell_size;
        p->velocity[a] = fabsf(p->velocity[a]) * .1f;
      }
      if (p->position[a] > limit - .1f * f->cell_size) {
        p->position[a] = limit - .1f * f->cell_size;
        p->velocity[a] = -fabsf(p->velocity[a]) * .1f;
      }
    }
    uint32_t cell = gvfx_cell(f, (int)(p->position[0] / f->cell_size),
                              (int)(p->position[1] / f->cell_size),
                              (int)(p->position[2] / f->cell_size));
    if (f->solid[cell]) {
      for (int a = 0; a < 3; a++) {
        p->position[a] -= p->velocity[a] * dt;
        p->velocity[a] = f->obstacle_velocity[cell * 3 + a];
      }
    }
  }
}
static void gvfx_shallow_step(GvfxFluid *f, float dt) {
  /* Conservative upwind height fluxes, reflective closed boundaries. */
  for (uint32_t y = 0; y < f->ny; y++)
    for (uint32_t x = 0; x < f->nx; x++) {
      uint32_t i = gvfx_cell(f, x, y, 0);
      float h = f->density[i], div = 0;
      if (f->solid[i]) {
        f->scratch[i] = h;
        continue;
      }
      for (int a = 0; a < 2; a++)
        for (int sign = -1; sign <= 1; sign += 2) {
          uint32_t j = gvfx_neighbour(f, x, y, 0, a, sign);
          if (j == i || f->solid[j])
            continue;
          float v = (f->velocity[i * 3 + a] + f->velocity[j * 3 + a]) * .5f;
          float donor = sign * v > 0 ? h : f->density[j];
          div += sign * v * donor / f->cell_size;
        }
      f->scratch[i] = fmaxf(0, h - dt * div);
    }
  memcpy(f->previous, f->density, (size_t)f->cells * sizeof(float));
  memcpy(f->density, f->scratch, (size_t)f->cells * sizeof(float));
  for (uint32_t y = 0; y < f->ny; y++)
    for (uint32_t x = 0; x < f->nx; x++) {
      uint32_t i = gvfx_cell(f, x, y, 0);
      for (int a = 0; a < 2; a++) {
        uint32_t l = gvfx_neighbour(f, x, y, 0, a, -1),
                 r = gvfx_neighbour(f, x, y, 0, a, 1);
        if (f->solid[i] || f->solid[l] || f->solid[r] || l == i || r == i)
          f->velocity[i * 3 + a] = 0;
        else
          f->velocity[i * 3 + a] -= fabsf(f->gravity) * dt *
                                    (f->density[r] - f->density[l]) /
                                    (2 * f->cell_size);
      }
    }
}
void gvfx_fluid_step(GvfxFluid *f, float dt) {
  if (!f || !isfinite(dt) || dt <= 0)
    return;
  f->dropped_time += fmaxf(0, dt - .1f);
  dt = fminf(dt, .1f);
  float vmax = 0, hmax = 0;
  for (uint32_t i = 0; i < f->cells; i++) {
    float v = 0;
    for (int a = 0; a < 3; a++)
      v += f->velocity[i * 3 + a] * f->velocity[i * 3 + a];
    vmax = fmaxf(vmax, sqrtf(v));
    hmax = fmaxf(hmax, f->density[i]);
  }
  f->max_velocity = vmax;
  float speed = vmax + (f->kind == 2 ? sqrtf(fabsf(f->gravity) * hmax) : 0);
  uint32_t sub = (uint32_t)ceilf(dt * speed / (.4f * f->cell_size));
  if (sub < 1)
    sub = 1;
  if (sub > 32) {
    float safe = 32 * .4f * f->cell_size / fmaxf(.00001f, speed);
    f->dropped_time += dt - safe;
    dt = safe;
    sub = 32;
  }
  f->substeps = sub;
  float step = dt / sub;
  for (uint32_t k = 0; k < sub; k++) {
    if (f->kind == 2)
      gvfx_shallow_step(f, step);
    else {
      if (f->kind == 1)
        gvfx_liquid_to_grid(f);
      else {
        gvfx_fluid_advect(f, f->density, 1, step);
        gvfx_fluid_advect(f, f->temperature, 1, step);
        gvfx_fluid_advect(f, f->fuel, 1, step);
        gvfx_fluid_advect(f, f->velocity, 3, step);
      }
      for (uint32_t i = 0; i < f->cells; i++)
        if (!f->solid[i]) {
          if (f->kind == 1)
            f->velocity[i * 3 + 1] += f->gravity * step;
          else {
            float burned = f->temperature[i] > .1f
                               ? f->fuel[i] * (1 - expf(-f->burn_rate * step))
                               : 0;
            f->fuel[i] -= burned;
            f->temperature[i] += burned * 4;
            f->density[i] =
                (f->density[i] + burned * .3f) * expf(-f->dissipation * step);
            f->temperature[i] *= expf(-.25f * step);
            f->velocity[i * 3 + 1] += f->buoyancy * f->temperature[i] * step;
          }
        }
      gvfx_fluid_project(f, step);
      if (f->kind == 1)
        gvfx_liquid_from_grid(f, step);
    }
    f->time += step;
    f->steps++;
  }
}
