#include "level_data.h"
#include <stdio.h>
#include <float.h>

#define VEC2F_EQUAL(A, B) (fabsf(A.x-B.x)<=FLT_EPSILON&&fabsf(A.y-B.y)<=FLT_EPSILON)
#define XY(V) (int)V.x, (int)V.y

/* Just for debbuging for now */
static uint32_t linedef_color = 0, sector_color = 0;

static bool polygon_vertices_contains_point(polygon*, vec2f);
static bool polygon_is_point_inside(polygon*, vec2f);
static void polygon_insert_point(polygon*, vec2f, vec2f, vec2f);

static bool sector_contains_vertex(sector*, vertex*, size_t);
static bool sector_connects_vertices(sector*, vertex*, vertex*);
static linedef* sector_add_linedef(sector*, linedef*);
static void sector_remove_linedef(sector*, linedef*);

static polygon* map_data_polygon_at_point(map_data*, vec2f);

static vertex* level_data_get_vertex(level_data*, vec2f);
static linedef* level_data_get_linedef(level_data*, sector*, vertex*, vertex*);
static sector* level_data_create_sector_from_polygon(level_data*, polygon*);

static void builder_step_find_polygon_intersections(map_data*, level_data*);
static void builder_step_remove_invalid_lines(map_data*, level_data*);
static void builder_step_configure_back_sectors(map_data*, level_data*);
static void builder_step_line_cleanup(map_data*, level_data*);

/*
 * Map data public API 
 */

void map_data_add_polygon(map_data *this, int32_t floor_height, int32_t ceiling_height, float light, size_t vertices_count, vec2f vertices[]) {
  register size_t i;

  printf("Add polygon (%d vertices) [%d, %d]:\n", vertices_count, floor_height, ceiling_height);

  this->polygons[this->polygons_count] = (polygon) {
    .vertices_count = vertices_count,
    .floor_height = floor_height,
    .ceiling_height = ceiling_height,
    .light = light
  };
  memcpy(this->polygons[this->polygons_count].vertices, vertices, vertices_count * sizeof(vec2f));

  for (i = 0; i < vertices_count; ++i) {
    printf("\t(%d, %d)\n", XY(this->polygons[this->polygons_count].vertices[i]));
  }

  this->polygons_count++;
}

level_data* map_data_build(map_data *this) {
  int i;

  level_data *level = malloc(sizeof(level_data));
  level->sectors_count = 0;
  level->linedefs_count = 0;
  level->vertices_count = 0;

  printf("Building level (0x%p) ...\n", level);

  /* ------------ */
  
  printf("\t1. Find all polygon intersections ...\n");
  
  builder_step_find_polygon_intersections(this, level);

  /* ------------ */
  
  printf("\t2. Creating sectors (from %d polys) ...\n", this->polygons_count);

  for (i = 0; i < this->polygons_count; ++i) {
    level_data_create_sector_from_polygon(level, &this->polygons[i]);
  }

  /* ------------ */

  printf("\t3. Remove invalid lines ...\n");

  builder_step_remove_invalid_lines(this, level);

  /* ------------ */

  printf("\t4. Configure back sectors ...\n");

  builder_step_configure_back_sectors(this, level);
  
  /* ------------ */

  printf("\t5. Final cleanup ...\n");

  builder_step_line_cleanup(this, level);

  /* ------------ */

  printf("DONE!\n");

  return level;
}

/*
 * Private methods
 */

/* POLYGON UTILS */

static bool polygon_vertices_contains_point(polygon *poly, vec2f point) {
  register size_t i;
  for (i = 0; i < poly->vertices_count; ++i) {
    if (math_length(vec2f_sub(poly->vertices[i], point)) < 1) {
      return true;
    }
  }
  return false;
}

static bool polygon_is_point_inside(polygon *this, vec2f point) {
  register int i, wn = 0;
  vec2f v0, v1;

  /* Winding number algorithm */
  for (i = 0; i < this->vertices_count; ++i) {
    v0 = this->vertices[i];
    v1 = this->vertices[(i+1)%this->vertices_count];

    if (v0.y <= point.y) {
      if (v1.y > point.y) {
        if (math_sign(v0, v1, point) > 0) {
          ++wn;
        }
      }
    } else {
      if (v1.y <= point.y) {
        if (math_sign(v0, v1, point) < 0) {
          --wn;
        }
      }
    }
  }

  return wn==1 || wn==-1;
}

static void polygon_insert_point(polygon *poly, vec2f point, /* between */ vec2f v0, vec2f v1) {
  register size_t i,j;

  for (i = 0; i < poly->vertices_count; ++i) {
    if ((VEC2F_EQUAL(poly->vertices[i], v0) && VEC2F_EQUAL(poly->vertices[(i+1)%poly->vertices_count], v1)) || (VEC2F_EQUAL(poly->vertices[i], v1) && VEC2F_EQUAL(poly->vertices[(i+1)%poly->vertices_count], v0))) {
      printf("\t\t\tInsert (%d,%d) between (%d,%d) and (%d,%d)\n", XY(point), XY(v0), XY(v1));
      for (j = poly->vertices_count; j > i; --j) {
        poly->vertices[j] = poly->vertices[j-1];
      }
      poly->vertices_count ++;
      poly->vertices[i+1] = point;
      return;
    }
  }
}

/* SECTOR UTILS */

static bool sector_contains_vertex(sector *this, vertex *v, size_t linedefs_count) {
  register size_t i;
  for (i = 0; i < (linedefs_count ? linedefs_count : this->linedefs_count); ++i) {
    if (this->linedefs[i]->v0 == v || this->linedefs[i]->v1 == v) {
      return true;
    }
  }
  return false;
}

static bool sector_connects_vertices(sector *this, vertex *v0, vertex *v1) {
  register size_t i;
  for (i = 0; i < this->linedefs_count; ++i) {
    if ((this->linedefs[i]->v0 == v0 && this->linedefs[i]->v1 == v1) || (this->linedefs[i]->v0 == v1 && this->linedefs[i]->v1 == v0)) {
      return true;
    }
  }
  return false;
}

static linedef* sector_add_linedef(sector *sect, linedef *line) {
  if (sect->linedefs_count) {
    sect->linedefs = realloc(sect->linedefs, sizeof(linedef*) * (sect->linedefs_count+1));
  } else {
    sect->linedefs = malloc(sizeof(linedef*));
  }
  sect->linedefs[sect->linedefs_count++] = line;
  return line;
}

static void sector_remove_linedef(sector *this, linedef *line) {
  register size_t i,j;
  for (i = 0; i < this->linedefs_count; ++i) {
    if (this->linedefs[i] == line) {
      this->linedefs_count--;
      for (j = i; j < this->linedefs_count; ++j) {
        this->linedefs[j] = this->linedefs[j+1];
      }
      this->linedefs = realloc(this->linedefs, sizeof(linedef*) * this->linedefs_count);
      return;
    }
  }
}

/* MAP DATA UTILS */

static polygon* map_data_polygon_at_point(map_data *this, vec2f point) {
  register size_t i = 0;

  for (i = 0; i < this->polygons_count; ++i) {
    if (polygon_is_point_inside(&this->polygons[i], point)) {
      return &this->polygons[i];
    }
  }

  return NULL;
}

/* LEVEL DATA UTILS */

/* FIND a vertex at given point OR CREATE a new one */
static vertex* level_data_get_vertex(level_data *level, vec2f point) {
  register size_t i;

  for (i = 0; i < level->vertices_count; ++i) {
    if (math_length(vec2f_sub(level->vertices[i].point, point)) < 1) {
      return &level->vertices[i];
    }
  }

  level->vertices[level->vertices_count] = (vertex) {
    .point = point
  };

  return &level->vertices[level->vertices_count++];
}

/* FIND a linedef with given vertices OR CREATE a new one */
static linedef* level_data_get_linedef(level_data *level, sector *sect, vertex *v0, vertex *v1) {
  register size_t i;
  linedef *line;

  /* Check for existing linedef with these vertices */
  for (i = 0; i < level->linedefs_count; ++i) {
    line = &level->linedefs[i];

    if ((line->v0 == v0 && line->v1 == v1) || (line->v0 == v1 && line->v1 == v0)) {
      line->side_sector[1] = sect;
      printf("\t\t\tRe-use linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d)\n", line, XY(v0->point), XY(v1->point), line->color);
      return line;
    }
  }

  level->linedefs[level->linedefs_count] = (linedef) {
    .v0 = v0,
    .v1 = v1,
    .side_sector[0] = sect,
    .side_sector[1] = 0,
    .color = linedef_color++
  };

  printf("\t\t\tNew linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d)\n", &level->linedefs[level->linedefs_count], XY(v0->point), XY(v1->point), linedef_color-1);

  return &level->linedefs[level->linedefs_count++];
}

static sector* level_data_create_sector_from_polygon(level_data *level, polygon *poly) {
  register size_t i;

  sector *sect = &level->sectors[level->sectors_count++];

  printf("\t\tNew sector (0x%p):\n", sect);

  sect->floor_height = poly->floor_height;
  sect->ceiling_height = poly->ceiling_height;
  sect->light = poly->light;
  sect->color = sector_color++;
  sect->linedefs = NULL;
  sect->linedefs_count = 0;

  for (i = 0; i < poly->vertices_count; ++i) {
    sector_add_linedef(
      sect,
      level_data_get_linedef(
        level,
        sect,
        level_data_get_vertex(level, poly->vertices[i]),
        level_data_get_vertex(level, poly->vertices[(i+1)%poly->vertices_count])
      )
    );
  }

  return sect;
}

/* BUILDER STEPS */

static void builder_step_find_polygon_intersections(map_data *this, level_data *level) {
  int i, j, vi, vj;
  polygon poly_i, poly_j;
  vec2f v0, v1, v2, v3, intersection;

  for (i = 0; i < this->polygons_count; ++i) {
    for (j = 0; j < this->polygons_count; ++j) {
      if (i == j) { continue; }

      poly_i = this->polygons[i];
      poly_j = this->polygons[j];

      for (vi = 0; vi < this->polygons[i].vertices_count; ++vi) {
        for (vj = 0; vj < this->polygons[j].vertices_count; ++vj) {
          v0 = this->polygons[i].vertices[vi];
          v1 = this->polygons[i].vertices[(vi+1) % this->polygons[i].vertices_count];
          v2 = this->polygons[j].vertices[vj];
          v3 = this->polygons[j].vertices[(vj+1) % this->polygons[j].vertices_count];

          /* Shared line or vertex */
          if ((math_length(vec2f_sub(v0, v2)) < 1 || math_length(vec2f_sub(v1, v3)) < 1) || (math_length(vec2f_sub(v0, v3)) < 1 || math_length(vec2f_sub(v1, v2)) < 1)) {
            continue;
          }

          bool skip_intersection_check = false;

          /* Add vertices from co-linear lines */

          if (math_point_on_line_segment(v0, v2, v3) && !polygon_vertices_contains_point(&poly_j, v0)) {
            printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", i, vi, XY(v0), XY(v2), XY(v3), j);
            polygon_insert_point(&poly_j, v0,   v2, v3);
            skip_intersection_check = true;
          }

          if (math_point_on_line_segment(v1, v2, v3) && !polygon_vertices_contains_point(&poly_j, v1)) {
            printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", i, vi, XY(v1), XY(v2), XY(v3), j);
            polygon_insert_point(&poly_j, v1,   v2, v3);
            skip_intersection_check = true;
          }

          if (math_point_on_line_segment(v2, v0, v1) && !polygon_vertices_contains_point(&poly_i, v2)) {
            printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", j, vj, XY(v2), XY(v0), XY(v1), i);
            polygon_insert_point(&poly_i, v2,   v0, v1);
            skip_intersection_check = true;
          }

          if (math_point_on_line_segment(v3, v0, v1) && !polygon_vertices_contains_point(&poly_i, v3)) {
            printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", j, vj, XY(v3), XY(v0), XY(v1), i);
            polygon_insert_point(&poly_i, v3,   v0, v1);
            skip_intersection_check = true;
          }

          if (!skip_intersection_check && math_find_line_intersection(v0, v1, v2, v3, &intersection, NULL)) {
            /*
             * TODO: Check if line completely splits some part of the polygon and make that into a separate sector?!
             *                                                       |
             * Like when having 2 long sectors crossing each other --+--
             *                                                       |
             * This should either split one of those into 2, or both where the middle bit becomes a separate poly as well.
             * Sounds horribly complicated though...
            */

            printf("\t\tSector %d line %d (%d,%d) <-> (%d,%d) intersects with sector %d line %d (%d,%d) <-> (%d,%d) at (%d,%d)\n",
              i,
              vi,
              XY(v0),
              XY(v1),
              j,
              vj,
              XY(v2),
              XY(v3),
              XY(intersection)
            );

            if (!polygon_vertices_contains_point(&poly_i, intersection)) {
              polygon_insert_point(&poly_i, intersection, v0, v1);
            }

            if (!polygon_vertices_contains_point(&poly_j, intersection)) {
              polygon_insert_point(&poly_j, intersection, v2, v3);
            }
          }
        }
      }

      this->polygons[i] = poly_i;
      this->polygons[j] = poly_j;
    }
  }
}

static void builder_step_remove_invalid_lines(map_data *this, level_data *level) {
  int i, j, k;
  sector *front, *back;
  linedef *line;

  for (i = 0; i < level->sectors_count; ++i) {
    front = &level->sectors[i];

    for (j = 0; j < level->sectors_count; ++j) {
      back = &level->sectors[j];
      
      if (back == front) { continue; }

      for (k = 0; k < front->linedefs_count; ++k) {
        line = front->linedefs[k];

        if (sector_contains_vertex(back, line->v0, 0) && sector_contains_vertex(back, line->v1, 0) && !sector_connects_vertices(back, line->v0, line->v1)) {
          printf("\t\tWill remove invalid line %d (%d,%d) <-> (%d,%d) from sector %d\n", k, XY(line->v0->point), XY(line->v1->point), i);
          sector_remove_linedef(front, line);
          k--;
        } else if (((sector_point_inside(back, line->v0->point) && !sector_contains_vertex(back, line->v0, 0)) || (sector_point_inside(back, line->v1->point) && !sector_contains_vertex(back, line->v1, 0))) && j > i) {
          printf("\t\tWill remove dangling line %d (%d,%d) <-> (%d,%d) from sector %d\n", k, XY(line->v0->point), XY(line->v1->point), i);
          sector_remove_linedef(front, line);
          k--;
        } else if (sector_connects_vertices(back, line->v0, line->v1) && i > j) {
          vec2f line_center = vec2f_mul(vec2f_add(line->v0->point, line->v1->point), 0.5f);
          vec2f line_dir = vec2f_sub(line->v0->point, line->v1->point);
          line_dir = vec2f_div(line_dir, math_length(line_dir));

          vec2f facing_0 = VEC2F(-line_dir.y, line_dir.x);
          vec2f facing_1 = VEC2F(line_dir.y, -line_dir.x);

          /*
           * If either side of the line is empty (no sector) this linedef shares a front face in 2 sectors.
           * Later (newer) sector will be front facing and the line will be removed from the other sector.
           */
          if (line->side_sector[0] && line->side_sector[1] && (map_data_polygon_at_point(this, vec2f_add(line_center, facing_0)) == NULL || map_data_polygon_at_point(this, vec2f_add(line_center, facing_1)) == NULL)) {
            printf("\t\tShared line (%d,%d) <-> (%d,%d) between sectors %d and %d\n", XY(line->v0->point), XY(line->v1->point), i, j);
            printf("\t\t  is empty of one side. Removing from sector %d\n", j);
            // Linedef will be removed from 'back' later on
            line->side_sector[0] = front;
            line->side_sector[1] = NULL;
          }

          continue;
        }
      }
    }
  }
}

static void builder_step_configure_back_sectors(map_data *this, level_data *level) {
  int i, j, k, new_count;
  sector *front, *back;
  linedef *line;

  for (j = 0; j < level->sectors_count; ++j) {
    front = &level->sectors[j];

    for (i = level->sectors_count - 1; i >= 0; --i) {
      back = &level->sectors[i];
      
      if (back == front) { continue; }

      new_count = back->linedefs_count;

      for (k = 0; k < front->linedefs_count; ++k) {
        line = front->linedefs[k];

        if (line->side_sector[0] && line->side_sector[1]) { continue; }
        if (sector_connects_vertices(back, line->v0, line->v1)) { continue; }
        
        if (polygon_is_point_inside(&this->polygons[i], line->v0->point) && polygon_is_point_inside(&this->polygons[i], line->v1->point)) {
          printf("\t\tAdd contained line %d (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, XY(line->v0->point), XY(line->v1->point), j, i);
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        } else if ((
             (sector_contains_vertex(back, line->v0, 0) && polygon_is_point_inside(&this->polygons[i], line->v1->point))
          || (sector_contains_vertex(back, line->v1, 0) && polygon_is_point_inside(&this->polygons[i], line->v0->point))
          ) && j > i
        ) {
          printf("\t\tAdd partial linedef %d (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, XY(line->v0->point), XY(line->v1->point), j, i);
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        }
      }

      back->linedefs_count = new_count;
    }
  }
}

static void builder_step_line_cleanup(map_data *this, level_data *level) {
  int i, k;
  sector *sect;
  linedef *line;

  for (i = 0; i < level->sectors_count; ++i) {
    sect = &level->sectors[i];

    for (k = 0; k < sect->linedefs_count; ++k) {
      line = sect->linedefs[k];

      if (line->side_sector[0] != sect && line->side_sector[1] == NULL) {
        printf("\t\tRemoving old linedef %d (%d,%d) <-> (%d,%d) from sector %d\n", k, XY(line->v0->point), XY(line->v1->point), i);
        sector_remove_linedef(sect, line);
        k--;
      }
    }
  }
}