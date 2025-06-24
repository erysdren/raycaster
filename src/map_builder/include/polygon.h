#ifndef RAYCAST_MAP_BUILDER_POLYGON_INCLUDED
#define RAYCAST_MAP_BUILDER_POLYGON_INCLUDED

#include "types.h"

#define VERTICES(...) M_NARG(__VA_ARGS__), (vec2f[]) { __VA_ARGS__ }

typedef struct polygon {
  int32_t floor_height,
          ceiling_height;
  float light;
  size_t vertices_count;
  vec2f vertices[32];
} polygon;

bool polygon_vertices_contains_point(polygon*, vec2f);
bool polygon_is_point_inside(polygon*, vec2f);
bool polygon_overlaps_polygon(polygon*, polygon*);
void polygon_insert_point(polygon*, vec2f, vec2f, vec2f);

#endif
