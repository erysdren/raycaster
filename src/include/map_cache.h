#ifndef RAYCASTER_MAP_CACHE_INCLUDED
#define RAYCASTER_MAP_CACHE_INCLUDED

#include "types.h"

struct level_data;
struct linedef;
struct cache_cell;

typedef struct cache_cell {
  uint8_t count;
  struct linedef **linedefs;
} cache_cell;

typedef struct map_cache {
  vec2f origin;
  uint32_t cell_count;
  uint16_t w, h;
  cache_cell *cells;
} map_cache;

void
map_cache_process_level_data(map_cache*, struct level_data*);

bool
map_cache_intersect_3d(const map_cache*, vec3f, vec3f);

#endif
