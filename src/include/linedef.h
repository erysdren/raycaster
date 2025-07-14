#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"
#include "light.h"
#include "texture.h"

static const float LINEDEF_SEGMENT_LENGTH_INV = 1.f / 128;

struct sector;

typedef enum {
  LINE_TEXTURE_TOP = 0,
  LINE_TEXTURE_MIDDLE,
  LINE_TEXTURE_BOTTOM
} linedef_side_texture;

typedef struct linedef_segment {
  vec2f p0, p1;
  light *lights[MAX_LIGHTS_PER_SURFACE];
  uint8_t lights_count;
} linedef_segment;

typedef struct linedef {
  vertex *v0, *v1;
  struct {
    struct sector *sector;
    texture_ref texture[3];
    linedef_segment *segments;
  } side[2];
  vec2f direction;
  int32_t max_floor_height,
          min_ceiling_height;
  uint16_t segments;
  float length, xmin, xmax, ymin, ymax;
} linedef;

void
linedef_update_floor_ceiling_limits(linedef*);

void
linedef_create_segments_for_side(linedef*, int side);

#endif
