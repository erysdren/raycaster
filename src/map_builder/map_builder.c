#include "level_data.h"
#include "map_builder.h"
#include <stdio.h>

#define XY(V) (int)V.x, (int)V.y

static void map_builder_step_find_polygon_intersections(map_builder*, level_data*);
static void map_builder_step_remove_invalid_lines(map_builder*, level_data*);
static void map_builder_step_configure_back_sectors(map_builder*, level_data*);
static void map_builder_step_line_cleanup(map_builder*, level_data*);

/*
 * Map data public API 
 */

void map_builder_add_polygon(map_builder *this, int32_t floor_height, int32_t ceiling_height, float light, size_t vertices_count, vec2f vertices[]) {
  M_DEBUG(register size_t i);

  M_DEBUG(printf("Add polygon (%d vertices) [%d, %d]:\n", vertices_count, floor_height, ceiling_height));

  this->polygons[this->polygons_count] = (polygon) {
    .vertices_count = vertices_count,
    .floor_height = floor_height,
    .ceiling_height = ceiling_height,
    .light = light
  };
  memcpy(this->polygons[this->polygons_count].vertices, vertices, vertices_count * sizeof(vec2f));

  M_DEBUG(for (i = 0; i < vertices_count; ++i) {
    printf("\t(%d, %d)\n", XY(this->polygons[this->polygons_count].vertices[i]));
  })

  this->polygons_count++;
}

level_data* map_builder_build(map_builder *this) {
  int i;

  level_data *level = malloc(sizeof(level_data));
  level->sectors_count = 0;
  level->linedefs_count = 0;
  level->vertices_count = 0;

  M_DEBUG(printf("Building level (0x%p) ...\n", level));

  /* ------------ */
  
  M_DEBUG(printf("\t1. Find all polygon intersections ...\n"));
  
  map_builder_step_find_polygon_intersections(this, level);

  /* ------------ */
  
  M_DEBUG(printf("\t2. Creating sectors (from %d polys) ...\n", this->polygons_count));

  for (i = 0; i < this->polygons_count; ++i) {
    level_data_create_sector_from_polygon(level, &this->polygons[i]);
  }

  /* ------------ */

  M_DEBUG(printf("\t3. Remove invalid lines ...\n"));

  map_builder_step_remove_invalid_lines(this, level);

  /* ------------ */

  M_DEBUG(printf("\t4. Configure back sectors ...\n"));

  map_builder_step_configure_back_sectors(this, level);
  
  /* ------------ */

  M_DEBUG(printf("\t5. Final cleanup ...\n"));

  map_builder_step_line_cleanup(this, level);

  /* ------------ */

  M_DEBUG(printf("DONE!\n"));

  return level;
}

/*
 * Private methods
 */

static void map_builder_step_find_polygon_intersections(map_builder *this, level_data *level) {
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
            M_DEBUG(printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", i, vi, XY(v0), XY(v2), XY(v3), j));
            polygon_insert_point(&poly_j, v0,   v2, v3);
            skip_intersection_check = true;
          }

          if (math_point_on_line_segment(v1, v2, v3) && !polygon_vertices_contains_point(&poly_j, v1)) {
            M_DEBUG(printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", i, vi, XY(v1), XY(v2), XY(v3), j));
            polygon_insert_point(&poly_j, v1,   v2, v3);
            skip_intersection_check = true;
          }

          if (math_point_on_line_segment(v2, v0, v1) && !polygon_vertices_contains_point(&poly_i, v2)) {
            M_DEBUG(printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", j, vj, XY(v2), XY(v0), XY(v1), i));
            polygon_insert_point(&poly_i, v2,   v0, v1);
            skip_intersection_check = true;
          }

          if (math_point_on_line_segment(v3, v0, v1) && !polygon_vertices_contains_point(&poly_i, v3)) {
            M_DEBUG(printf("\t\tPoly %d vertex %d (%d,%d) is on line (%d,%d) <-> (%d,%d) of poly %d\n", j, vj, XY(v3), XY(v0), XY(v1), i));
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

            M_DEBUG(printf("\t\tSector %d line %d (%d,%d) <-> (%d,%d) intersects with sector %d line %d (%d,%d) <-> (%d,%d) at (%d,%d)\n",
              i,
              vi,
              XY(v0),
              XY(v1),
              j,
              vj,
              XY(v2),
              XY(v3),
              XY(intersection)
            ));

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

static void map_builder_step_remove_invalid_lines(map_builder *this, level_data *level) {
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

        if (sector_references_vertex(back, line->v0, 0) && sector_references_vertex(back, line->v1, 0) && !sector_connects_vertices(back, line->v0, line->v1)) {
          M_DEBUG(printf("\t\tWill remove invalid line %d (%d,%d) <-> (%d,%d) from sector %d\n", k, XY(line->v0->point), XY(line->v1->point), i));
          sector_remove_linedef(front, line);
          k--;
        } else if (((sector_point_inside(back, line->v0->point) && !sector_references_vertex(back, line->v0, 0)) || (sector_point_inside(back, line->v1->point) && !sector_references_vertex(back, line->v1, 0))) && j > i) {
          M_DEBUG(printf("\t\tWill remove dangling line %d (%d,%d) <-> (%d,%d) from sector %d\n", k, XY(line->v0->point), XY(line->v1->point), i));
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
          if (line->side_sector[0] && line->side_sector[1] && (map_builder_polygon_at_point(this, vec2f_add(line_center, facing_0)) == NULL || map_builder_polygon_at_point(this, vec2f_add(line_center, facing_1)) == NULL)) {
            M_DEBUG(printf("\t\tShared line (%d,%d) <-> (%d,%d) between sectors %d and %d\n", XY(line->v0->point), XY(line->v1->point), i, j));
            M_DEBUG(printf("\t\t  is empty of one side. Removing from sector %d\n", j));
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

static void map_builder_step_configure_back_sectors(map_builder *this, level_data *level) {
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
          M_DEBUG(printf("\t\tAdd contained line %d (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, XY(line->v0->point), XY(line->v1->point), j, i));
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        } else if ((
             (sector_references_vertex(back, line->v0, 0) && polygon_is_point_inside(&this->polygons[i], line->v1->point))
          || (sector_references_vertex(back, line->v1, 0) && polygon_is_point_inside(&this->polygons[i], line->v0->point))
          ) && j > i
        ) {
          M_DEBUG(printf("\t\tAdd partial linedef %d (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, XY(line->v0->point), XY(line->v1->point), j, i));
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        }
      }

      back->linedefs_count = new_count;
    }
  }
}

static void map_builder_step_line_cleanup(map_builder *this, level_data *level) {
  int i, k;
  sector *sect;
  linedef *line;

  for (i = 0; i < level->sectors_count; ++i) {
    sect = &level->sectors[i];

    for (k = 0; k < sect->linedefs_count; ++k) {
      line = sect->linedefs[k];

      if (line->side_sector[0] != sect && line->side_sector[1] == NULL) {
        M_DEBUG(printf("\t\tRemoving old linedef %d (%d,%d) <-> (%d,%d) from sector %d\n", k, XY(line->v0->point), XY(line->v1->point), i));
        sector_remove_linedef(sect, line);
        k--;
      }
    }
  }
}
