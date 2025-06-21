#include "level_data.h"
#include <stdio.h>

/* Just for debbuging for now */
static uint32_t linedef_color = 0, sector_color = 0;

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
    printf("\t(%d, %d)\n", (int)this->polygons[this->polygons_count].vertices[i].x, (int)this->polygons[this->polygons_count].vertices[i].y);
  }

  this->polygons_count++;
}

/* POLYGON UTILS */

M_INLINED bool poly_contains_point(polygon *poly, vec2f point) {
  register size_t i;
  for (i = 0; i < poly->vertices_count; ++i) {
    if (math_length(vec2f_sub(poly->vertices[i], point)) < 1) {
      return true;
    }
  }
  return false;
}

M_INLINED void poly_insert_point(polygon *poly, size_t index, vec2f point) {
  register size_t i;
  for (i = poly->vertices_count; i > index; --i) {
    printf("Moving vertex %d to %d\n", i-1, i);
    poly->vertices[i] = poly->vertices[i-1];
  }
  poly->vertices_count ++;
  printf("Inserting (%d,%d) at %d\n", (int)point.x, (int)point.y, index);
  poly->vertices[index] = point;
}

/* SECTOR UTILS */

M_INLINED bool sector_contains_vertex(sector *this, vertex *v) {
  register size_t i;
  for (i = 0; i < this->linedefs_count; ++i) {
    if (this->linedefs[i]->v0 == v || this->linedefs[i]->v1 == v) {
      return true;
    }
  }
  return false;
}

M_INLINED void sector_remove_linedef(sector *this, linedef *line) {
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

/* FIND a vertex at given point OR CREATE a new one */
static vertex* get_vertex(level_data *level, vec2f point) {
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
static linedef* get_linedef(level_data *level, sector *sect, vertex *v0, vertex *v1) {
  register size_t i;
  linedef *line;

  /* Check for existing linedef with these vertices */
  for (i = 0; i < level->linedefs_count; ++i) {
    line = &level->linedefs[i];

    if ((line->v0 == v0 && line->v1 == v1) || (line->v0 == v1 && line->v1 == v0)) {
      line->side_sector[1] = sect;
      printf("\t\t\tRe-use linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d)\n", line, (int)v0->point.x, (int)v0->point.y, (int)v1->point.x, (int)v1->point.y, line->color);
      return line;
    }
  }

  level->linedefs[level->linedefs_count] = (linedef) {
    .v0 = v0,
    .v1 = v1,
    .side_sector[0] = sect,
    .color = linedef_color++
  };

  printf("\t\t\tNew linedef (0x%p): (%d,%d) <-> (%d,%d) (Color: %d)\n", &level->linedefs[level->linedefs_count], (int)v0->point.x, (int)v0->point.y, (int)v1->point.x, (int)v1->point.y, linedef_color-1);

  return &level->linedefs[level->linedefs_count++];
}

static linedef* add_linedef(level_data *level, sector *sect, linedef *line) {
  if (sect->linedefs_count) {
    sect->linedefs = realloc(sect->linedefs, sizeof(linedef*) * (sect->linedefs_count+1));
  } else {
    sect->linedefs = malloc(sizeof(linedef*));
  }
  sect->linedefs[sect->linedefs_count++] = line;
  return line;
}

static sector* add_sector(level_data *level, polygon *poly) {
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
    add_linedef(level, sect, get_linedef(level, sect, get_vertex(level, poly->vertices[i]), get_vertex(level, poly->vertices[(i+1)%poly->vertices_count])));
  }

  return sect;
}

level_data* map_data_build(map_data *this) {
  register size_t i, j, vi, vj, k, new_count, i_added, j_added;
  vec2f v0, v1, v2, v3, intersection;
  sector *front, *back;
  linedef *line;
  polygon poly_i, poly_j;

  level_data *level = malloc(sizeof(level_data));
  level->sectors_count = 0;
  level->linedefs_count = 0;
  level->vertices_count = 0;

  printf("Building level (0x%p) ...\n", level);

  printf("\t1. Find all polygon intersections ...\n");

  for (i = 0; i < this->polygons_count; ++i) {
    for (j = 0; j < this->polygons_count; ++j) {
      if (i == j) { continue; }

      poly_i = this->polygons[i];
      poly_j = this->polygons[j];

      for (vi = 0, i_added = 0; vi < this->polygons[i].vertices_count; ++vi) {
        for (vj = 0, j_added = 0; vj < this->polygons[j].vertices_count; ++vj) {
          v0 = this->polygons[i].vertices[vi];
          v1 = this->polygons[i].vertices[(vi+1) % this->polygons[i].vertices_count];
          v2 = this->polygons[j].vertices[vj];
          v3 = this->polygons[j].vertices[(vj+1) % this->polygons[j].vertices_count];

          /* Shared line or vertex */
          if ((math_length(vec2f_sub(v0, v2)) < 1 || math_length(vec2f_sub(v1, v3)) < 1) || (math_length(vec2f_sub(v0, v3)) < 1 || math_length(vec2f_sub(v1, v2)) < 1)) {
            continue;
          }

          if (math_lines_intersect(v0, v1, v2, v3, &intersection, NULL)) {
            printf("\t\tSector %d line [%d](%d,%d) <-> [%d](%d,%d) intersects with sector %d line [%d](%d,%d) <-> [%d](%d,%d) at (%d,%d)\n",
              i,
              vi,
              (int)v0.x, (int)v0.y,
              (vi+1) % this->polygons[i].vertices_count,
              (int)v1.x, (int)v1.y,
              j,
              vj,
              (int)v2.x, (int)v2.y,
              (vj+1) % this->polygons[j].vertices_count,
              (int)v3.x, (int)v3.y,
              (int)intersection.x, (int)intersection.y
            );

            if (!poly_contains_point(&poly_i, intersection)) {
              poly_insert_point(&poly_i, vi+1+(i_added++), intersection);
            }

            if (!poly_contains_point(&poly_j, intersection)) {
              poly_insert_point(&poly_j, vj+1+(j_added++), intersection);
            }
          }
        }
      }

      this->polygons[i] = poly_i;
      this->polygons[j] = poly_j;
    }
  }

  printf("\t2. Creating sectors (from %d polys) ...\n", this->polygons_count);

  for (i = 0; i < this->polygons_count; ++i) {
    add_sector(level, &this->polygons[i]);
  }

  printf("\t3. Link contained sectors ...\n");

  for (i = 0; i < level->sectors_count; ++i) {
    back = &level->sectors[i];

    for (j = 0; j < level->sectors_count; ++j) {
      front = &level->sectors[j];
      
      if (back == front) { continue; }

      new_count = back->linedefs_count;
      
      for (k = 0; k < front->linedefs_count; ++k) {
        line = front->linedefs[k];

        if (line->side_sector[0] && line->side_sector[1]) { continue; }
        if (sector_contains_vertex(back, line->v0) && sector_contains_vertex(back, line->v1)) {
          if (line->side_sector[1] != back) {
            printf("Remove line? (%d,%d) <-> (%d,%d)\n", (int)line->v0->point.x, (int)line->v0->point.y, (int)line->v1->point.x, (int)line->v1->point.y);
            sector_remove_linedef(front, line);
            k--;
          }
          continue;
        }

        if (sector_point_inside(back, line->v0->point) && sector_point_inside(back, line->v1->point)) {
          printf("\t\tAdd contained linedef [%d] (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, (int)line->v0->point.x, (int)line->v0->point.y, (int)line->v1->point.x, (int)line->v1->point.y, j, i);
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        } else if ((sector_contains_vertex(back, line->v0) && sector_point_inside(back, line->v1->point))
          || (sector_contains_vertex(back, line->v1) && sector_point_inside(back, line->v0->point))) {
          printf("\t\tAdd partial linedef [%d] (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, (int)line->v0->point.x, (int)line->v0->point.y, (int)line->v1->point.x, (int)line->v1->point.y, j, i);
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        }
      }

      back->linedefs_count = new_count;

      /*for (k = 0; k < front->linedefs_count; ++k) {
        line = front->linedefs[k];

        if (line->side_sector[0] && line->side_sector[1]) { continue; }
        if (sector_contains_vertex(back, line->v0) && sector_contains_vertex(back, line->v1)) { continue; }

        if (sector_contains_vertex(back, line->v0) && sector_contains_vertex(back, line->v1) && sector_contains_vertex(back, line->v0))
      }*/
    }
  }

  /*printf("\t4. Remove invalid linedefs ...\n");

  for (i = 0; i < level->sectors_count; ++i) {
    sect = &level->sectors[i];
  }*/

  printf("DONE!\n");

  return level;
}