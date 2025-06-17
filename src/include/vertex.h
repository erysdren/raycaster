#ifndef RAYCAST_VERTEX_INCLUDED
#define RAYCAST_VERTEX_INCLUDED

#include "types.h"

typedef struct {
  vec2f point;
  uint32_t last_visibility_check_tick;
  bool visible;
} vertex;

#endif
