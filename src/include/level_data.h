#ifndef RAYCAST_LEVEL_DATA_INCLUDED
#define RAYCAST_LEVEL_DATA_INCLUDED

#include "sector.h"

typedef struct {
  size_t sectors_count,
         linedefs_count;
  linedef linedefs[2048*4];
  sector sectors[2048];
} level_data;


/* Structures for building the level */

#define VERTICES(...) M_NARG(__VA_ARGS__), (vertex[]) { __VA_ARGS__ }

typedef struct {
  size_t vertices_count;
  vertex vertices[16];
  int32_t floor_height,
          ceiling_height;
} polygon;

typedef struct {
  size_t polygons_count;
  polygon polygons[2048];
} map_data;

void map_data_add_polygon(map_data*, int32_t floor_height, int32_t ceiling_height, size_t vertices_count, vertex vertices[]);
level_data* map_data_build(map_data*);

#endif
