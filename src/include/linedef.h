#ifndef RAYCAST_LINEDEF_INCLUDED
#define RAYCAST_LINEDEF_INCLUDED

#include "vertex.h"

#define LDEF(...) ((linedef) { __VA_ARGS__ })

struct sector;

typedef struct {
  vertex v0,
         v1;
  struct sector *side_sector[2];
  uint32_t color;
} linedef;

#endif
