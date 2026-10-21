/* Graphene VFX reference executor. All allocations occur at creation/program
   publication. A tick never allocates and never calls application code. */
#include "vfx.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static float gvfx_sat(float x) { return fminf(1, fmaxf(0, x)); }
static uint32_t gvfx_hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}
static float gvfx_random(uint32_t x) {
  return (float)(gvfx_hash(x) >> 8) * (1.0f / 16777216.0f);
}
static float gvfx_get(GvfxEmitter *e, uint32_t slot, uint32_t component) {
  return e->scope ? e->scope[component]
                  : e->soa[(size_t)component * e->capacity + slot];
}
static void gvfx_set(GvfxEmitter *e, uint32_t slot, uint32_t component,
                     float x) {
  if (e->scope)
    e->scope[component] = isfinite(x) ? x : 0;
  else
    e->soa[(size_t)component * e->capacity + slot] = isfinite(x) ? x : 0;
}
static uint32_t gvfx_particle_key(GvfxEmitter *e, uint32_t slot) {
  uint32_t low =
      (uint32_t)gvfx_get(e, slot, 19) | ((uint32_t)gvfx_get(e, slot, 21) << 16);
  uint32_t high =
      (uint32_t)gvfx_get(e, slot, 22) | ((uint32_t)gvfx_get(e, slot, 23) << 16);
  return low ^ gvfx_hash(high);
}
GvfxEmitter *gvfx_create(uint32_t capacity, uint32_t seed) {
  if (!capacity || capacity > 4194304u)
    return NULL;
  GvfxEmitter *e = calloc(1, sizeof(*e));
  if (!e)
    return NULL;
  e->capacity = capacity;
  e->seed = seed;
  e->lifetime = 3;
  e->size = .1f;
  e->speed = 1;
  e->looping = true;
  e->duration = 5;
  e->direction[1] = 1;
  e->color[0] = e->color[1] = e->color[2] = e->color[3] = 1;
  e->soa = calloc((size_t)capacity * GVFX_COMPONENTS, sizeof(float));
  e->free_ids = malloc((size_t)capacity * sizeof(uint32_t));
  e->alive = malloc((size_t)capacity * sizeof(uint32_t));
  e->event_capacity = capacity < 4096 ? capacity : 4096;
  e->events = calloc(e->event_capacity, sizeof(GvfxEvent));
  if (!e->soa || !e->free_ids || !e->alive || !e->events) {
    gvfx_destroy(e);
    return NULL;
  }
  gvfx_restart(e);
  return e;
}
void gvfx_destroy(GvfxEmitter *e) {
  if (e) {
    free(e->soa);
    free(e->free_ids);
    free(e->alive);
    free(e->events);
    free(e->program);
    free(e);
  }
}
void gvfx_restart(GvfxEmitter *e) {
  memset(e->soa, 0, (size_t)e->capacity * GVFX_COMPONENTS * sizeof(float));
  for (uint32_t i = 0; i < e->capacity; i++)
    e->free_ids[i] = e->capacity - i - 1;
  e->count = 0;
  e->free_count = e->capacity;
  e->next_id = 1;
  e->generation = 0;
  e->tick = 0;
  e->time = 0;
  e->accumulator = 0;
  e->spawn_fraction = 0;
  e->burst_left = e->burst;
  e->event_count = 0;
  e->dropped_events = 0;
  e->dropped_spawn = 0;
  e->paused = false;
  e->emitting = true;
  e->finished = false;
  e->started = false;
  e->cycle_time = 0;
  e->dropped_time = 0;
  e->steps = e->spawned = e->expired = 0;
  memset(e->system_data, 0, sizeof e->system_data);
  memset(e->emitter_data, 0, sizeof e->emitter_data);
}
static bool gvfx_index(float f, uint32_t maximum) {
  return isfinite(f) && f >= 0 && f < (float)maximum && floorf(f) == f;
}
bool gvfx_program(GvfxEmitter *e, const float *words, size_t count) {
  if (!e || !words || count < GVFX_WORDS || count > 1048576 ||
      !gvfx_index(words[0], 65537))
    return false;
  for (size_t i = 0; i < count; i++)
    if (!isfinite(words[i]))
      return false;
  uint32_t n = (uint32_t)words[0];
  if (16u + (size_t)n * GVFX_WORDS > count)
    return false;
  for (uint32_t i = 0; i < n; i++) {
    const float *p = words + 16u + (size_t)i * GVFX_WORDS;
    if (!gvfx_index(p[0], 37) || p[0] < 1 || !gvfx_index(p[1], 32) ||
        !gvfx_index(p[2], 32) || !gvfx_index(p[3], 32) ||
        !gvfx_index(p[8], 32) || !gvfx_index(p[9], 8) || !gvfx_index(p[10], 8))
      return false;
    if (p[0] == GVFX_RANDOM && !gvfx_index(p[4], 16777216))
      return false;
    if (p[0] == GVFX_CONTEXT && !gvfx_index(p[4], 5))
      return false;
    if (p[0] == GVFX_STORE && (p[9] == 4 || p[9] == 5))
      return false;
    if (p[0] == GVFX_PARAMETER && (!gvfx_index(p[4], GVFX_PARAMETER_WORDS / 4)))
      return false;
    if (p[0] == GVFX_CURVE) {
      if (!gvfx_index(p[4], (uint32_t)count) || !gvfx_index(p[5], 4097) ||
          p[5] < 1 || p[4] < 16u + n * GVFX_WORDS ||
          (size_t)p[4] + (size_t)p[5] * 8 > count)
        return false;
      for (uint32_t j = 0; j < (uint32_t)p[5]; j++)
        if (j &&
            words[(uint32_t)p[4] + j * 8] < words[(uint32_t)p[4] + (j - 1) * 8])
          return false;
    }
    for (int k = 0; k < 16; k++)
      if (!isfinite(p[k]))
        return false;
  }
  float *copy = malloc(count * sizeof(float));
  if (!copy)
    return false;
  memcpy(copy, words, count * sizeof(float));
  free(e->program);
  e->program = copy;
  e->program_words = (uint32_t)count;
  e->instructions = n;
  return true;
}
void gvfx_emit_event(GvfxEmitter *e, uint32_t kind, uint32_t slot,
                     float extra) {
  if (e->event_count == e->event_capacity) {
    e->dropped_events++;
    return;
  }
  GvfxEvent *v = &e->events[e->event_count++];
  memset(v, 0, sizeof(*v));
  v->value[0] = (float)kind;
  v->value[1] = gvfx_get(e, slot, 19);
  v->value[2] = e->time;
  v->value[3] = extra;
  for (int k = 0; k < 3; k++) {
    v->value[4 + k] = gvfx_get(e, slot, k);
    v->value[8 + k] = gvfx_get(e, slot, 4 + k);
  }
  v->value[7] = (float)(e->seed & 65535u);
  v->value[11] = (float)(e->seed >> 16);
  v->value[12] = gvfx_get(e, slot, 20);
  v->value[13] = gvfx_get(e, slot, 21);
  v->value[14] = gvfx_get(e, slot, 22);
  v->value[15] = gvfx_get(e, slot, 23);
}
/* One register file is reused for a particle. Instructions are shared by the
   GPU backend; deterministic random is keyed by particle identity and seed. */
static void gvfx_execute(GvfxEmitter *e, uint32_t slot, uint32_t stage,
                         float dt) {
  float r[GVFX_REGISTERS][4] = {{0}};
  for (uint32_t ip = 0; ip < e->instructions; ip++) {
    const float *p = e->program + 16 + (size_t)ip * GVFX_WORDS;
    if ((uint32_t)p[10] != stage)
      continue;
    uint32_t op = (uint32_t)p[0], d = (uint32_t)p[1], a = (uint32_t)p[2],
             b = (uint32_t)p[3], c = (uint32_t)p[8], attr = (uint32_t)p[9];
    float out[4] = {0};
    float len = 0;
    for (int k = 0; k < 3; k++)
      len += r[a][k] * r[a][k];
    len = sqrtf(len);
    if (op == GVFX_STORE) {
      for (int k = 0; k < 4; k++)
        gvfx_set(e, slot, attr * 4 + k, r[a][k]);
      continue;
    }
    if (op == GVFX_PLANE) {
      float h = p[4], bounce = gvfx_sat(p[5]);
      if (gvfx_get(e, slot, 1) < h) {
        gvfx_set(e, slot, 1, h);
        gvfx_set(e, slot, 5, fabsf(gvfx_get(e, slot, 5)) * bounce);
        gvfx_emit_event(e, GVFX_COLLISION, slot, bounce);
      }
      continue;
    }
    if (op == GVFX_ATTRACT) {
      float dir[3], dist = 0;
      for (int k = 0; k < 3; k++) {
        dir[k] = r[a][k] - gvfx_get(e, slot, k);
        dist += dir[k] * dir[k];
      }
      dist = sqrtf(dist);
      if (dist > .00001f)
        for (int k = 0; k < 3; k++)
          gvfx_set(e, slot, 4 + k,
                   gvfx_get(e, slot, 4 + k) + dir[k] / dist * p[4] * dt);
      continue;
    }
    if (op == GVFX_KILL) {
      if (r[a][0] > .5f)
        gvfx_set(e, slot, 3, gvfx_get(e, slot, 7));
      continue;
    }
    if (op == GVFX_EVENT) {
      if (r[a][0] > .5f)
        gvfx_emit_event(e, GVFX_USER, slot, p[4]);
      continue;
    }
    if (op == GVFX_SPHERE || op == GVFX_BOX || op == GVFX_CONE) {
      uint32_t seed = e->seed ^ gvfx_particle_key(e, slot) ^ (ip * 0x9e3779b9u);
      float x = gvfx_random(seed) * 2 - 1, y = gvfx_random(seed + 1) * 2 - 1,
            z = gvfx_random(seed + 2) * 2 - 1;
      if (op == GVFX_SPHERE) {
        float n = sqrtf(x * x + y * y + z * z);
        float radius = p[4] * cbrtf(gvfx_random(seed + 3));
        if (n > .00001f) {
          x = x / n * radius;
          y = y / n * radius;
          z = z / n * radius;
        }
      }
      if (op == GVFX_BOX) {
        x *= p[4];
        y *= p[5];
        z *= p[6];
      }
      if (op == GVFX_CONE) {
        float theta = gvfx_random(seed) * 6.28318530718f;
        float cosphi = 1 - gvfx_random(seed + 1) * (1 - cosf(p[4]));
        float s = sqrtf(fmaxf(0, 1 - cosphi * cosphi));
        x = cosf(theta) * s * p[5];
        y = cosphi * p[5];
        z = sinf(theta) * s * p[5];
      }
      out[0] = x;
      out[1] = y;
      out[2] = z;
    } else if (op == GVFX_CURVE) {
      uint32_t base = (uint32_t)p[4], n = (uint32_t)p[5], j = 0;
      float t = r[a][0];
      while (j + 1 < n && e->program[base + (j + 1) * 8] <= t)
        j++;
      uint32_t next = j + 1 < n ? j + 1 : j;
      float x0 = e->program[base + j * 8], x1 = e->program[base + next * 8];
      float f = x1 > x0 ? gvfx_sat((t - x0) / (x1 - x0)) : 0;
      for (int k = 0; k < 4; k++)
        out[k] = e->program[base + j * 8 + 4 + k] * (1 - f) +
                 e->program[base + next * 8 + 4 + k] * f;
    } else if (op == GVFX_CROSS) {
      out[0] = r[a][1] * r[b][2] - r[a][2] * r[b][1];
      out[1] = r[a][2] * r[b][0] - r[a][0] * r[b][2];
      out[2] = r[a][0] * r[b][1] - r[a][1] * r[b][0];
    } else if (op == GVFX_DOT) {
      for (int k = 0; k < 3; k++)
        out[0] += r[a][k] * r[b][k];
      out[1] = out[2] = out[3] = out[0];
    } else
      for (int k = 0; k < 4; k++) {
        float x = r[a][k], y = r[b][k];
        switch (op) {
        case GVFX_CONST:
          out[k] = p[4 + k];
          break;
        case GVFX_LOAD:
          out[k] = gvfx_get(e, slot, attr * 4 + k);
          break;
        case GVFX_ADD:
          out[k] = x + y;
          break;
        case GVFX_SUB:
          out[k] = x - y;
          break;
        case GVFX_MUL:
          out[k] = x * y;
          break;
        case GVFX_DIV:
          out[k] = fabsf(y) > 1e-12f ? x / y : 0;
          break;
        case GVFX_MIX:
          out[k] = x + (y - x) * r[c][k];
          break;
        case GVFX_CLAMP:
          out[k] = fminf(p[5], fmaxf(p[4], x));
          break;
        case GVFX_SIN:
          out[k] = sinf(x);
          break;
        case GVFX_COS:
          out[k] = cosf(x);
          break;
        case GVFX_NORMALIZE:
          out[k] = k < 3 && len > 1e-12f ? x / len : 0;
          break;
        case GVFX_LENGTH:
          out[k] = len;
          break;
        case GVFX_RANDOM:
          out[k] =
              gvfx_random(e->seed ^ gvfx_particle_key(e, slot) ^
                          ((uint32_t)p[4] * 7919u) ^ ((uint32_t)k * 104729u));
          break;
        case GVFX_NOISE:
          out[k] = sinf(x * 12.9898f + y * 78.233f + p[4]);
          break;
        case GVFX_CONTEXT:
          out[k] =
              (uint32_t)p[4] == 0   ? e->time
              : (uint32_t)p[4] == 1 ? dt
              : (uint32_t)p[4] == 2
                  ? gvfx_get(e, slot, 3) / fmaxf(.00001f, gvfx_get(e, slot, 7))
              : (uint32_t)p[4] == 3 ? gvfx_get(e, slot,
                                               k == 0   ? 19
                                               : k == 1 ? 21
                                               : k == 2 ? 22
                                                        : 23)
              : (uint32_t)p[4] == 4 ? (k < 3 ? e->origin[k] : 0)
                                    : 0;
          break;
        case GVFX_PARAMETER:
          out[k] = e->parameters[(uint32_t)p[4] * 4 + k];
          break;
        case GVFX_SELECT:
          out[k] = r[c][0] > .5f ? x : y;
          break;
        case GVFX_LESS:
          out[k] = x < y;
          break;
        case GVFX_GREATER:
          out[k] = x > y;
          break;
        case GVFX_ABS:
          out[k] = fabsf(x);
          break;
        case GVFX_MIN:
          out[k] = fminf(x, y);
          break;
        case GVFX_MAX:
          out[k] = fmaxf(x, y);
          break;
        case GVFX_POWER:
          out[k] = powf(fmaxf(0, x), y);
          break;
        case GVFX_FLOOR:
          out[k] = floorf(x);
          break;
        case GVFX_FRACT:
          out[k] = x - floorf(x);
          break;
        default:
          break;
        }
      }
    for (int k = 0; k < 4; k++)
      r[d][k] = isfinite(out[k]) ? out[k] : 0;
  }
}
uint32_t gvfx_spawn(GvfxEmitter *e, uint32_t requested) {
  uint32_t count = requested < e->free_count ? requested : e->free_count;
  e->dropped_spawn += requested - count;
  for (uint32_t j = 0; j < count; j++) {
    uint32_t slot = e->free_ids[--e->free_count];
    e->alive[e->count++] = slot;
    for (uint32_t k = 0; k < GVFX_COMPONENTS; k++)
      gvfx_set(e, slot, k, 0);
    for (int k = 0; k < 3; k++) {
      gvfx_set(e, slot, k, e->local_space ? 0 : e->origin[k]);
      gvfx_set(e, slot, 4 + k, e->direction[k] * e->speed);
      gvfx_set(e, slot, 12 + k, e->size);
    }
    gvfx_set(e, slot, 7, e->lifetime);
    for (int k = 0; k < 4; k++)
      gvfx_set(e, slot, 8 + k, e->color[k]);
    gvfx_set(e, slot, 19, (float)(e->next_id & 65535u));
    gvfx_set(e, slot, 21, (float)(e->next_id >> 16));
    gvfx_set(e, slot, 22, (float)(e->generation & 65535u));
    gvfx_set(e, slot, 23, (float)(e->generation >> 16));
    if (++e->next_id == 0)
      e->generation++;
    gvfx_set(e, slot, 20, (float)e->seed);
    gvfx_execute(e, slot, GVFX_SPAWN, GVFX_FIXED_STEP);
    for (int k = 0; k < 3; k++)
      gvfx_set(e, slot, 16 + k, gvfx_get(e, slot, k));
    gvfx_emit_event(e, GVFX_BIRTH, slot, 0);
    gvfx_execute(e, slot, GVFX_EVENT_STAGE, GVFX_FIXED_STEP);
    e->spawned++;
  }
  uint32_t previous = e->next_id;
  e->next_id += requested - count;
  if (e->next_id < previous)
    e->generation++;
  return count;
}
void gvfx_step(GvfxEmitter *e, float dt) {
  if (!e || e->paused || !isfinite(dt) || dt <= 0)
    return;
  e->event_count = 0;
  e->dropped_time += fmaxf(0, dt - .25f);
  e->accumulator += fminf(dt, .25f);
  for (uint32_t sub = 0; sub < 15 && e->accumulator + 1e-7f >= GVFX_FIXED_STEP;
       sub++) {
    if (!e->started) {
      e->scope = e->system_data;
      gvfx_execute(e, 0, GVFX_SYSTEM_SPAWN, GVFX_FIXED_STEP);
      e->scope = e->emitter_data;
      gvfx_execute(e, 0, GVFX_EMITTER_SPAWN, GVFX_FIXED_STEP);
      e->scope = NULL;
      e->started = true;
    }
    e->accumulator = fmaxf(0, e->accumulator - GVFX_FIXED_STEP);
    e->time += GVFX_FIXED_STEP;
    e->tick++;
    e->steps++;

    e->scope = e->system_data;
    gvfx_execute(e, 0, GVFX_SYSTEM_UPDATE, GVFX_FIXED_STEP);
    e->scope = e->emitter_data;
    gvfx_execute(e, 0, GVFX_EMITTER_UPDATE, GVFX_FIXED_STEP);
    e->scope = NULL;
    for (uint32_t i = 0; i < e->count;) {
      uint32_t slot = e->alive[i];
      for (int k = 0; k < 3; k++)
        gvfx_set(e, slot, 16 + k, gvfx_get(e, slot, k));
      gvfx_set(e, slot, 3, gvfx_get(e, slot, 3) + GVFX_FIXED_STEP);
      gvfx_execute(e, slot, GVFX_UPDATE, GVFX_FIXED_STEP);
      gvfx_set(e, slot, 5, gvfx_get(e, slot, 5) + e->gravity * GVFX_FIXED_STEP);
      float damping = expf(-fmaxf(0, e->drag) * GVFX_FIXED_STEP);
      for (int k = 0; k < 3; k++) {
        float v = gvfx_get(e, slot, 4 + k) * damping;
        gvfx_set(e, slot, 4 + k, v);
        gvfx_set(e, slot, k, gvfx_get(e, slot, k) + v * GVFX_FIXED_STEP);
      }
      uint32_t events_before = e->event_count;
      gvfx_execute(e, slot, GVFX_SIMULATION, GVFX_FIXED_STEP);
      if (e->event_count != events_before)
        gvfx_execute(e, slot, GVFX_EVENT_STAGE, GVFX_FIXED_STEP);
      if (gvfx_get(e, slot, 3) >= gvfx_get(e, slot, 7)) {
        gvfx_emit_event(e, GVFX_DEATH, slot, 0);
        gvfx_execute(e, slot, GVFX_EVENT_STAGE, GVFX_FIXED_STEP);
        e->free_ids[e->free_count++] = slot;
        e->alive[i] = e->alive[--e->count];
        e->expired++;
      } else
        i++;
    }
    e->cycle_time += GVFX_FIXED_STEP;
    bool window = e->cycle_time + 1e-6f >= e->delay &&
                  (e->duration <= 0 || e->cycle_time < e->delay + e->duration);
    if (e->emitting && window) {
      e->spawn_fraction += fmaxf(0, e->rate) * GVFX_FIXED_STEP;
      uint32_t n = (uint32_t)fminf(e->spawn_fraction, 4194304);
      e->spawn_fraction -= n;
      if (e->burst_left) {
        n += e->burst_left;
        e->burst_left = 0;
      }
      gvfx_spawn(e, n);
    }
    if (e->emitting && e->duration > 0 &&
        e->cycle_time >= e->delay + e->duration) {
      if (e->looping) {
        e->cycle_time = fmodf(e->cycle_time, e->delay + e->duration);
        e->burst_left = e->burst;
        e->scope = e->emitter_data;
        gvfx_execute(e, 0, GVFX_EMITTER_SPAWN, GVFX_FIXED_STEP);
        e->scope = NULL;
      } else
        e->emitting = false;
    }
    e->finished = !e->emitting && !e->count;
  }
}
