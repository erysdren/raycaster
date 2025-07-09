#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"
#include "light.h"
#include "texture.h"

struct sector;

typedef enum {
  LINE_TEXTURE_TOP = 0,
  LINE_TEXTURE_MIDDLE,
  LINE_TEXTURE_BOTTOM
} linedef_side_texture;

typedef struct linedef {
  vertex *v0, *v1;
  struct {
    struct sector *sector;
    uint8_t lights_count;
    light *lights[MAX_LIGHTS_PER_SURFACE];
    texture_ref texture[3];
  } side[2];
  int32_t max_floor_height,
          min_ceiling_height;
  float length, xmin, xmax, ymin, ymax;

#ifdef LINE_VIS_CHECK
  uint32_t last_visible_tick;
#endif
} linedef;

void
linedef_update_floor_ceiling_limits(linedef*);

#endif
