#ifndef RAYCAST_SECTOR_INCLUDED
#define RAYCAST_SECTOR_INCLUDED

#include "linedef.h"
#include "maths.h"
#include "macros.h"
#include <string.h>
#include <stdint.h>

#define MAX_SECTOR_LINEDEFS 32

#define LINEDEFS(...) M_NARG(__VA_ARGS__), (linedef[]) { __VA_ARGS__ }

/* Just for debbuging for now */
static uint32_t linedef_color = 0, sector_color = 0;

typedef struct sector {
  int32_t floor_height,
          ceiling_height;
  linedef linedefs[MAX_SECTOR_LINEDEFS];
  size_t linedefs_count;
  uint32_t color;
} sector;

M_INLINED void sector_init(
  sector *this,
  int32_t floor_height,
  int32_t ceiling_height,
  size_t linedefs_count,
  linedef linedefs[]
) {
  register size_t i;

  this->floor_height = floor_height;
  this->ceiling_height = ceiling_height;
  this->linedefs_count = linedefs_count;
  this->color = sector_color++;
  
  memcpy(this->linedefs, linedefs, sizeof(linedef) * linedefs_count);

  for (i = 0; i < linedefs_count; ++i) {
    this->linedefs[i].color = linedef_color++;
    this->linedefs[i].side_sector[LINEDEF_FRONT] = this;
  }
}

M_INLINED bool sector_point_inside(const sector *this, vec2f point) {
  register int i, wn = 0;
  register const linedef *ld;

  /* Winding number algorithm */
  for (i = 0; i < this->linedefs_count; ++i) {
    ld = &this->linedefs[i];

    if (ld->v0.point.y <= point.y) {
      if (ld->v1.point.y > point.y) {
        if (math_sign(ld->v0.point, ld->v1.point, point) > 0) {
          ++wn;
        }
      }
    } else {
      if (ld->v1.point.y <= point.y) {
        if (math_sign(ld->v0.point, ld->v1.point, point) < 0) {
          --wn;
        }
      }
    }
  }

  return wn != 0;
}

#endif
