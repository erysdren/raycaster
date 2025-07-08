#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"
#include "light.h"

struct sector;

typedef struct linedef {
  vertex *v0, *v1;
  struct {
    struct sector *sector;
    uint8_t lights_count;
    light *lights[MAX_LIGHTS_PER_SURFACE];
  } side[2];
  uint32_t color; // TODO: remove once textures are in
  int32_t max_floor_height,
          min_ceiling_height;
  float xmin, xmax, ymin, ymax;

#ifdef LINE_VIS_CHECK
  uint32_t last_visible_tick;
#endif
} linedef;

void
linedef_update_floor_ceiling_limits(linedef*);

#endif
