/* Included once by Graphene's native translation unit. */
#include "vfx.c"
typedef struct GvfxOwned {
  size_t id;
  GvfxEmitter *emitter;
  struct GvfxOwned *next;
} GvfxOwned;
static GvfxOwned *gvfx_owned;
static size_t gvfx_next_id = 1;
static char gvfx_message[256];
static GvfxEmitter *gvfx_find(size_t id) {
  for (GvfxOwned *p = gvfx_owned; p; p = p->next)
    if (p->id == id)
      return p->emitter;
  snprintf(gvfx_message, sizeof gvfx_message, "Expired particle handle");
  return NULL;
}
encore_str graphene_vfx_error(void) { return graphene_string(gvfx_message); }
size_t graphene_vfx_create(uint32_t capacity, uint32_t seed) {
  GvfxEmitter *e = gvfx_create(capacity, seed);
  if (!e) {
    snprintf(gvfx_message, sizeof gvfx_message,
             "Unable to allocate particle pool (capacity 1..4194304)");
    return 0;
  }
  GvfxOwned *p = malloc(sizeof(*p));
  if (!p) {
    gvfx_destroy(e);
    return 0;
  }
  p->id = gvfx_next_id++;
  p->emitter = e;
  p->next = gvfx_owned;
  gvfx_owned = p;
  return p->id;
}
bool graphene_vfx_close(size_t id) {
  GvfxOwned **p = &gvfx_owned;
  while (*p && (*p)->id != id)
    p = &(*p)->next;
  if (!*p)
    return false;
  GvfxOwned *v = *p;
  *p = v->next;
  gvfx_destroy(v->emitter);
  free(v);
  return true;
}
bool graphene_vfx_configure(size_t id, const float *v, size_t n) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e || !v || n < 21)
    return false;
  for (size_t i = 0; i < n; i++)
    if (!isfinite(v[i]))
      return false;
  if (v[0] < 0 || v[0] > 10000000 || v[1] <= 0 || v[1] > 3600 || v[3] < 0 ||
      v[3] > 100000 || v[5] < 0 || v[17] < 0 || v[17] > 4194304 || v[18] < 0 ||
      v[19] < 0)
    return false;
  e->rate = v[0];
  e->lifetime = v[1];
  e->speed = v[2];
  e->size = v[3];
  e->gravity = v[4];
  e->drag = v[5];
  memcpy(e->origin, v + 6, 3 * sizeof(float));
  memcpy(e->direction, v + 9, 3 * sizeof(float));
  memcpy(e->color, v + 12, 4 * sizeof(float));
  e->local_space = v[16] > .5f;
  e->burst = (uint32_t)v[17];
  e->duration = v[18];
  e->delay = v[19];
  e->looping = v[20] > .5f;
  return true;
}
bool graphene_vfx_publish(size_t id, const float *v, size_t n) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e)
    return false;
  bool ok = gvfx_program(e, v, n);
  if (!ok)
    snprintf(gvfx_message, sizeof gvfx_message, "Invalid VFX bytecode");
  return ok;
}
bool graphene_vfx_parameters(size_t id, const float *v, size_t n) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e || n > GVFX_PARAMETER_WORDS)
    return false;
  for (size_t i = 0; i < n; i++)
    if (!isfinite(v[i]))
      return false;
  memcpy(e->parameters, v, n * sizeof(float));
  return true;
}
bool graphene_vfx_tick(size_t id, float delta) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e)
    return false;
  gvfx_step(e, delta);
  return true;
}
bool graphene_vfx_action(size_t id, uint32_t action) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e)
    return false;
  switch (action) {
  case 0:
    e->paused = true;
    break;
  case 1:
    e->paused = false;
    break;
  case 2:
    e->emitting = false;
    break;
  case 3:
    e->emitting = false;
    e->count = 0;
    e->free_count = e->capacity;
    e->finished = true;
    for (uint32_t i = 0; i < e->capacity; i++)
      e->free_ids[i] = e->capacity - i - 1;
    break;
  case 4:
    gvfx_restart(e);
    break;
  default:
    return false;
  }
  return true;
}
uint64_t graphene_vfx_stat(size_t id, uint32_t field) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e)
    return 0;
  switch (field) {
  case 0:
    return e->count;
  case 1:
    return e->capacity;
  case 2:
    return e->spawned;
  case 3:
    return e->expired;
  case 4:
    return e->dropped_spawn;
  case 5:
    return e->dropped_events;
  case 6:
    return (uint64_t)e->capacity * (32 * sizeof(float) + 2 * sizeof(uint32_t)) +
           (uint64_t)e->program_words * sizeof(float);
  case 7:
    return e->steps;
  case 8:
    return e->finished;
  case 9:
    return e->event_count;
  default:
    return 0;
  }
}
bool graphene_vfx_copy(size_t id, float *out, size_t words) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e || !out || words < (size_t)e->count * 32)
    return false;
  for (uint32_t c = 0; c < 32; c++)
    for (uint32_t i = 0; i < e->count; i++)
      out[(size_t)c * e->count + i] = gvfx_get(e, e->alive[i], c);
  return true;
}
bool graphene_vfx_events(size_t id, float *out, size_t words) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e || !out || words < (size_t)e->event_count * 16)
    return false;
  memcpy(out, e->events, (size_t)e->event_count * sizeof(GvfxEvent));
  return true;
}
bool graphene_vfx_clear_events(size_t id) {
  GvfxEmitter *e = gvfx_find(id);
  if (!e)
    return false;
  e->event_count = 0;
  return true;
}
float graphene_vfx_time(size_t id) {
  GvfxEmitter *e = gvfx_find(id);
  return e ? e->time : 0;
}
#include "shaders.c"

bool graphene_vfx_validate(const float *words, size_t count) {
  GvfxEmitter temporary = {0};
  bool valid = gvfx_program(&temporary, words, count);
  free(temporary.program);
  return valid;
}
