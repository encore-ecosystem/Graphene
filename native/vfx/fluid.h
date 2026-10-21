#ifndef GRAPHENE_VFX_FLUID_H
#define GRAPHENE_VFX_FLUID_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Cell-centred reference for gases and shallow water; PIC/FLIP couples
   particles to the same pressure projection. GPU solvers use the same
   world-space units. */
typedef struct {
  float position[3], velocity[3];
} GvfxLiquidParticle;
typedef struct {
  uint32_t nx, ny, nz, cells, kind, iterations, substeps, particle_count,
      particle_capacity;
  float cell_size, time, flip, gravity, dissipation, buoyancy, burn_rate,
      max_velocity, divergence_before, divergence_after, dropped_time;
  float *velocity, *previous, *scratch, *density, *temperature, *fuel,
      *pressure, *pressure_next, *divergence, *weights, *obstacle_velocity;
  uint8_t *solid, *liquid;
  GvfxLiquidParticle *particles;
  uint64_t steps;
} GvfxFluid;
GvfxFluid *gvfx_fluid_create(uint32_t nx, uint32_t ny, uint32_t nz,
                             uint32_t kind, float cell_size,
                             uint32_t particles);
void gvfx_fluid_destroy(GvfxFluid *f);
void gvfx_fluid_project(GvfxFluid *f, float dt);
void gvfx_fluid_step(GvfxFluid *f, float dt);
void gvfx_fluid_source(GvfxFluid *f, const float position[3], float radius,
                       float density, float heat, float fuel,
                       const float velocity[3], float dt);
void gvfx_fluid_obstacle(GvfxFluid *f, const float position[3], float radius,
                         const float velocity[3]);
#endif
