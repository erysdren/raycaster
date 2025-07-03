#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"
#include "light.h"

struct sector;

typedef struct linedef {
  vertex *v0, *v1;
  struct sector *side_sector[2];
  uint32_t color;
  uint8_t lights_count[2];
  light *lights[2][MAX_LIGHTS_PER_SURFACE];
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
