#ifndef RAYCAST_VERTEX_INCLUDED
#define RAYCAST_VERTEX_INCLUDED

#include "types.h"

#define VERTEX(X, Y) ((vertex) { .point.x = X, .point.y = Y })

typedef struct {
  vec2f point;
} vertex;

#endif
