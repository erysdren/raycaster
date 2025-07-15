#ifndef RAYCASTER_MAP_CACHE_INCLUDED
#define RAYCASTER_MAP_CACHE_INCLUDED

#include "types.h"
#include "light.h"

#define CELL_SIZE 76.f

struct level_data;
struct linedef;
struct map_cache_cell;

typedef struct map_cache_cell {
  uint8_t count, lights_count;
  struct linedef **linedefs;
  light *lights[MAX_LIGHTS_PER_SURFACE];
} map_cache_cell;

typedef struct map_cache {
  vec2f origin;
  uint32_t cell_count;
  uint16_t w, h;
  map_cache_cell *cells;
} map_cache;

void
map_cache_process_level_data(map_cache*, struct level_data*);

void
map_cache_process_light(map_cache*, struct light*, vec3f);

bool
map_cache_intersect_3d(const map_cache*, vec3f, vec3f);

M_INLINED map_cache_cell *
map_cache_cell_at(const map_cache *this, const vec2f world_position)
{
  const vec2f local_position = vec2f_sub(world_position, this->origin);
  uint16_t x = local_position.x / CELL_SIZE;
  uint16_t y = local_position.y / CELL_SIZE;
  if (x < 0 || y < 0 || x >= this->w || y >= this->h) {
    return NULL;
  }
  return &this->cells[y*this->w+x];
}

#endif
