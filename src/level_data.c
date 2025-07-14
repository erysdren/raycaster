#include "level_data.h"
#include "polygon.h"
#include <assert.h>

#define XY(V) (int)V.x, (int)V.y

static bool
linedef_contains_light(const linedef*, int, const light*);

static bool
sector_floor_contains_light(const sector*, const light*);

static bool
sector_ceiling_contains_light(const sector*, const light*);

/* FIND a vertex at given point OR CREATE a new one */
vertex* level_data_get_vertex(level_data *this, vec2f point)
{
  register size_t i;

  if (!this->vertices_count) {
    this->min = VEC2F(FLT_MAX, FLT_MAX);
    this->max = VEC2F(-FLT_MAX, -FLT_MAX);
  }

  for (i = 0; i < this->vertices_count; ++i) {
    if (math_length(vec2f_sub(this->vertices[i].point, point)) < 1) {
      return &this->vertices[i];
    }
  }

  this->vertices[this->vertices_count] = (vertex) {
    .point = point
  };

  if (point.x < this->min.x) { this->min.x = point.x; }
  if (point.y < this->min.y) { this->min.y = point.y; }
  if (point.x > this->max.x) { this->max.x = point.x; }
  if (point.y > this->max.y) { this->max.y = point.y; }

  return &this->vertices[this->vertices_count++];
}

/* FIND a linedef with given vertices OR CREATE a new one */
linedef* level_data_get_linedef(level_data *this, sector *sect, vertex *v0, vertex *v1, texture_ref texture)
{
  register size_t i;
  linedef *line;

  /* Check for existing linedef with these vertices */
  for (i = 0; i < this->linedefs_count; ++i) {
    line = &this->linedefs[i];

    if ((line->v0 == v0 && line->v1 == v1) || (line->v0 == v1 && line->v1 == v0)) {
      line->side[1].sector = sect;
      
      line->side[1].texture[0] = line->side[0].texture[0];
      line->side[1].texture[1] = line->side[0].texture[1];
      line->side[1].texture[2] = line->side[0].texture[2];

      line->side[0].texture[0] = texture;
      line->side[0].texture[1] = texture;
      line->side[0].texture[2] = texture;

      IF_DEBUG(printf("\t\tRe-use linedef (0x%p): (%d,%d) <-> (%d,%d) (Front: 0x%p, Back: 0x%p)\n",
        line, XY(v0->point), XY(v1->point), line->side[0].sector, line->side[1].sector
      ))
      return line;
    }
  }

  this->linedefs[this->linedefs_count] = (linedef) {
    .v0 = v0,
    .v1 = v1,
    .side[0] = {
      .sector = sect,
      .texture[0] = texture,
      .texture[1] = texture,
      .texture[2] = texture,
      .lights_count = 0
    },
    .side[1] = {
      .sector = NULL,
      .texture[0] = TEXTURE_NONE,
      .texture[1] = TEXTURE_NONE,
      .texture[2] = TEXTURE_NONE,
      .lights_count = 0
    },
    .direction = vec2f_sub(v1->point, v0->point),
    .length = math_vec2f_distance(v0->point, v1->point),
    .xmin = fminf(v0->point.x, v1->point.x),
    .xmax = fmaxf(v0->point.x, v1->point.x),
    .ymin = fminf(v0->point.y, v1->point.y),
    .ymax = fmaxf(v0->point.y, v1->point.y)
  };

  IF_DEBUG(printf("\t\tNew linedef (0x%p): (%d,%d) <-> (%d,%d) (Front: 0x%p, Back: 0x%p)\n",
    &this->linedefs[this->linedefs_count], XY(v0->point), XY(v1->point), sect, NULL
  ))

  return &this->linedefs[this->linedefs_count++];
}

sector* level_data_create_sector_from_polygon(level_data *this, polygon *poly)
{
  register size_t i;

  sector *sect = &this->sectors[this->sectors_count++];

  IF_DEBUG(printf("\tNew sector (0x%p):\n", sect))

  sect->floor.height = poly->floor_height;
  sect->floor.texture = poly->floor_texture;
  sect->floor.lights_count = 0;
  sect->ceiling.height = poly->ceiling_height;
  sect->ceiling.texture = poly->ceiling_texture;
  sect->ceiling.lights_count = 0;
  sect->brightness = poly->brightness;
  sect->linedefs = NULL;
  sect->linedefs_count = 0;

#ifdef LINE_VIS_CHECK
  sect->visible_linedefs = NULL;
  sect->visible_linedefs_count = 0;
#endif

  for (i = 0; i < poly->vertices_count; ++i) {
    linedef_update_floor_ceiling_limits(
      sector_add_linedef(
        sect,
        level_data_get_linedef(
          this,
          sect,
          level_data_get_vertex(this, poly->vertices[i]),
          level_data_get_vertex(this, poly->vertices[(i+1)%poly->vertices_count]),
          poly->wall_texture
        )
      )
    );
  }

  return sect;
}

light*
level_data_add_light(level_data *this, vec3f pos, float r, float s) {
  if (this->lights_count == 64) {
    return NULL;
  }

  light *new_light = &this->lights[this->lights_count++];
  new_light->position = pos;
  new_light->radius = r;
  new_light->radius_sq = r*r;
  new_light->radius_sq_inverse = 1.f / new_light->radius_sq;
  new_light->strength = s;
  new_light->level = this;

  level_data_update_lights(this);

  return new_light;
}

void
level_data_update_lights(level_data *this)
{
  int i, si, li, side;
  float sign, lz;
  light *lite;
  sector *sect;
  linedef *line;
  vec2f pos2d;
  bool sector_floor_lit, sector_ceiling_lit;

  for (si = 0; si < this->sectors_count; ++si) {
    sect = &this->sectors[si];
    sect->floor.lights_count = 0;
    sect->ceiling.lights_count = 0;

    for (li = 0; li < sect->linedefs_count; ++li) {
      line = sect->linedefs[li];
      line->side[0].lights_count = 0;
      line->side[1].lights_count = 0;
    }
  }

  for (i = 0; i < this->lights_count; ++i) {
    lite = &this->lights[i];

    pos2d = VEC2F(lite->position.x, lite->position.y);

    /* Find all sectors the light circle touches */
    for (si = 0; si < this->sectors_count; ++si) {
      sect = &this->sectors[si];
      sector_floor_lit = sector_floor_contains_light(sect, lite);
      sector_ceiling_lit = sector_ceiling_contains_light(sect, lite);

      if (sector_point_inside(sect, pos2d)) {
        if (!sector_floor_lit && sect->floor.lights_count < MAX_LIGHTS_PER_SURFACE) {
          if ((lz = lite->position.z - sect->floor.height) && lz > 0 && lz <= lite->radius) {
            sect->floor.lights[sect->floor.lights_count++] = lite;
          }
          sector_floor_lit = true;
        }
        if (!sector_ceiling_lit && sect->ceiling.lights_count < MAX_LIGHTS_PER_SURFACE) {
          if ((lz = sect->ceiling.height - lite->position.z) && lz > 0 && lz <= lite->radius) {
            sect->ceiling.lights[sect->ceiling.lights_count++] = lite;
          }
          sector_ceiling_lit = true;
        }
      }

      for (li = 0; li < sect->linedefs_count; ++li) {
        line = sect->linedefs[li];
        side = sect==line->side[0].sector?0:1;
        sign = math_sign(line->v0->point, line->v1->point, pos2d);

  #ifdef DYNAMIC_SHADOWS
        /*
         * In dynamic shadow mode, a surface is lightable when the line simply
         * intersects the light circle. Pixel perfect ray check is performed
         * in the renderer later on.
         */
        if (math_line_segment_point_distance(line->v0->point, line->v1->point, pos2d) <= lite->radius) {
          if (!sector_floor_lit && sect->floor.lights_count < MAX_LIGHTS_PER_SURFACE) {
            if ((lz = lite->position.z - sect->floor.height) && lz > 0 && lz <= lite->radius) {
              sect->floor.lights[sect->floor.lights_count++] = lite;
            }
            sector_floor_lit = true;
          }

          if (!sector_ceiling_lit && sect->ceiling.lights_count < MAX_LIGHTS_PER_SURFACE) {
            if ((lz = sect->ceiling.height - lite->position.z) && lz > 0 && lz <= lite->radius) {
              sect->ceiling.lights[sect->ceiling.lights_count++] = lite;
            }
            sector_ceiling_lit = true;
          }

          if ((side == 0 ? (sign < 0) : (sign > 0)) &&
              line->side[side].lights_count < MAX_LIGHTS_PER_SURFACE &&
              !linedef_contains_light(line, side, lite)
          ) {
            line->side[side].lights[line->side[side].lights_count++] = lite;
          }
        }
  #else
        /*
         * In non-shadowed version, a surface is lightable when any of its
         * vertices has a line of sight to the light.
         */

        /* 1 - Check floor polygon vertices */
        if (!sector_lit && sect->lights_count < MAX_LIGHTS_PER_SURFACE) {
          if (!level_data_intersect_3d(this, VEC3F(line->v0->point.x, line->v0->point.y, sect->floor.height), lite->position, sect)) {
            sect->lights[sect->lights_count++] = lite;
            sector_lit = true;
          }
        }

        /* 2 - Check four corners of the wall */
        if ((side == 0 ? (sign < 0) : (sign > 0)) &&
            line->side[side].lights_count < MAX_LIGHTS_PER_SURFACE &&
            !linedef_contains_light(line, side, lite)
        ) {
          if (!level_data_intersect_3d(this, VEC3F(line->v0->point.x, line->v0->point.y, sect->floor.height), lite->position, sect) ||
              !level_data_intersect_3d(this, VEC3F(line->v1->point.x, line->v1->point.y, sect->floor.height), lite->position, sect) ||
              !level_data_intersect_3d(this, VEC3F(line->v0->point.x, line->v0->point.y, sect->ceiling.height), lite->position, sect) ||
              !level_data_intersect_3d(this, VEC3F(line->v0->point.x, line->v0->point.y, sect->ceiling.height), lite->position, sect
          )) {
            line->side[side].lights[line->side[side].lights_count++] = lite;
          }
        }
  #endif
      }
    }
  }
}

M_INLINED bool
sector_visited(sector *element, size_t n, sector **history)
{
  if (!n) { return false; }
  sector **p = history;
  sector **end = history + n;
  do {
    if (*p == element) { return true; }
  } while (++p != end);
  return false;
}

bool
level_data_intersect_3d(const level_data *this, vec3f p0, vec3f p1, const sector *start)
{
  assert(start);
  register size_t li = 0, sh=0;
  int any_hits;
  linedef *line;
  sector *sect = (sector*)start, *back;
  sector *history[64];
  vec2f p0_2 = VEC2F(p0.x, p0.y);
  vec2f p1_2 = VEC2F(p1.x, p1.y);
  float det, z, z0 = p0.z, z1 = p1.z;

  // printf("\nstart %p\n", start);

  do {
    if (sh == 64) {
      printf("history limit hit (%d)!\n", sh);
      return false;
    }
    for (li = 0, back = NULL, any_hits = 0; li < sect->linedefs_count; ++li) {
      line = sect->linedefs[li];

      /*sign[0] = math_sign(line->v0->point, line->v1->point, p0_2);
      sign[1] = math_sign(line->v0->point, line->v1->point, p1_2);

      // Both ray points are on the same side of the linedef, cannot intersect
      if ((sign[0] < 0 && sign[1] < 0) || (sign[0] > 0 && sign[1] > 0)) {
        continue;
      }*/

      if (math_find_line_intersection(p0_2, p1_2, line->v0->point, line->v1->point, NULL, &det) && det > MATHS_EPSILON) {
        back = line->side[0].sector == sect ? line->side[1].sector : line->side[0].sector;
        if (sector_visited(back, sh, history)) {
          continue;
        }
        if (!back) {
          return true;
        }
        any_hits = 1;
        z = z0+(z1-z0)*det;
        if (z < back->floor.height || z > back->ceiling.height) {
          return true;
        }
        // printf("%p line %d back %p\n", sect, li, back);
        history[sh++] = back;
        sect = back;
        break;
      }
    }
    if (!any_hits) {
      if (sector_point_inside(sect, p1_2)) {
        return false;
      }
      break;
    }
  } while(sect);

  return false;
}

static bool
linedef_contains_light(const linedef *this, int side, const light *lt)
{
  size_t i;
  for (i = 0; i < this->side[side].lights_count; ++i) {
    if (this->side[side].lights[i] == lt) {
      return true;
    }
  }
  return false;
}

static bool
sector_floor_contains_light(const sector *this, const light *lt)
{
  size_t i;
  for (i = 0; i < this->floor.lights_count; ++i) {
    if (this->floor.lights[i] == lt) {
      return true;
    }
  }
  return false;
}

static bool
sector_ceiling_contains_light(const sector *this, const light *lt)
{
  size_t i;
  for (i = 0; i < this->ceiling.lights_count; ++i) {
    if (this->ceiling.lights[i] == lt) {
      return true;
    }
  }
  return false;
}
