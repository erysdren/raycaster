#ifndef RAYCAST_SECTOR_INCLUDED
#define RAYCAST_SECTOR_INCLUDED

#include "linedef.h"
#include "maths.h"
#include "macros.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_SECTOR_LINEDEFS 32

#define LINEDEFS(...) M_NARG(__VA_ARGS__), (linedef[]) { __VA_ARGS__ }

typedef struct sector {
  int32_t floor_height,
          ceiling_height;
  linedef *linedefs[MAX_SECTOR_LINEDEFS];
  size_t linedefs_count;
  uint32_t color;
  uint32_t last_visibility_check_tick;
} sector;

M_INLINED bool sector_point_inside(const sector *this, vec2f point) {
  register int i, wn = 0;
  register const linedef *line;

  /* Winding number algorithm */
  for (i = 0; i < this->linedefs_count; ++i) {
    line = this->linedefs[i];

    if (line->v0->point.y <= point.y) {
      if (line->v1->point.y > point.y) {
        if (math_sign(line->v0->point, line->v1->point, point) > 0) {
          ++wn;
        }
      }
    } else {
      if (line->v1->point.y <= point.y) {
        if (math_sign(line->v0->point, line->v1->point, point) < 0) {
          --wn;
        }
      }
    }
  }

  return wn==1 || wn==-1;
}

#endif
