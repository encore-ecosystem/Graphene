#ifndef GRAPHENE_VFX_H
#define GRAPHENE_VFX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Shared CPU/SPIR-V ABI. Values are vec4; particle storage is component-major
   SoA. Programs contain sixteen float words per instruction, followed by curve
   data. */
#define GVFX_COMPONENTS 32u
#define GVFX_REGISTERS 32u
#define GVFX_WORDS 16u
#define GVFX_PARAMETER_WORDS 256u
#define GVFX_FIXED_STEP (1.0f / 60.0f)
enum {
  GVFX_CONST = 1,
  GVFX_LOAD,
  GVFX_STORE,
  GVFX_ADD,
  GVFX_SUB,
  GVFX_MUL,
  GVFX_DIV,
  GVFX_MIX,
  GVFX_CLAMP,
  GVFX_SIN,
  GVFX_COS,
  GVFX_NORMALIZE,
  GVFX_LENGTH,
  GVFX_DOT,
  GVFX_RANDOM,
  GVFX_NOISE,
  GVFX_CONTEXT,
  GVFX_PARAMETER,
  GVFX_CURVE,
  GVFX_SELECT,
  GVFX_LESS,
  GVFX_GREATER,
  GVFX_CROSS,
  GVFX_SPHERE,
  GVFX_BOX,
  GVFX_CONE,
  GVFX_PLANE,
  GVFX_ATTRACT,
  GVFX_KILL,
  GVFX_EVENT,
  GVFX_ABS,
  GVFX_MIN,
  GVFX_MAX,
  GVFX_POWER,
  GVFX_FLOOR,
  GVFX_FRACT
};
enum {
  GVFX_SPAWN = 0,
  GVFX_UPDATE = 1,
  GVFX_SYSTEM_SPAWN = 2,
  GVFX_SYSTEM_UPDATE = 3,
  GVFX_EMITTER_SPAWN = 4,
  GVFX_EMITTER_UPDATE = 5,
  GVFX_EVENT_STAGE = 6,
  GVFX_SIMULATION = 7
};
enum { GVFX_BIRTH = 1, GVFX_DEATH = 2, GVFX_COLLISION = 3, GVFX_USER = 4 };
typedef struct {
  float value[16];
} GvfxEvent;
typedef struct {
  float *soa;
  uint32_t *free_ids, *alive;
  uint32_t capacity, count, free_count, next_id, generation, seed, tick;
  float time, accumulator, spawn_fraction;
  float rate, lifetime, speed, size, gravity, drag;
  float origin[3], direction[3], color[4];
  float *program;
  uint32_t instructions, program_words;
  float parameters[GVFX_PARAMETER_WORDS];
  GvfxEvent *events;
  uint32_t event_count, event_capacity, dropped_events, dropped_spawn;
  uint32_t burst, burst_left;
  float duration, delay, cycle_time, dropped_time;
  float system_data[32], emitter_data[32], *scope;
  bool paused, emitting, finished, local_space, looping, started;
  uint64_t steps, spawned, expired;
} GvfxEmitter;
GvfxEmitter *gvfx_create(uint32_t capacity, uint32_t seed);
void gvfx_destroy(GvfxEmitter *e);
bool gvfx_program(GvfxEmitter *e, const float *words, size_t count);
void gvfx_restart(GvfxEmitter *e);
void gvfx_step(GvfxEmitter *e, float dt);
uint32_t gvfx_spawn(GvfxEmitter *e, uint32_t count);
void gvfx_emit_event(GvfxEmitter *e, uint32_t kind, uint32_t slot, float extra);
#endif
