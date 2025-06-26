#include "level_data.h"
#include "map_builder.h"
#include <gpc.h>
#include <stdio.h>
#include <assert.h>

#define XY(V) (int)V.x, (int)V.y

static void map_builder_step_find_polygon_intersections(map_builder*, level_data*);
static void map_builder_step_configure_back_sectors(map_builder*, level_data*);
/*
 * Map data public API 
 */

void map_builder_add_polygon(map_builder *this, int32_t floor_height, int32_t ceiling_height, float light, size_t vertices_count, vec2f vertices[])
{
  M_DEBUG(register size_t i);
  M_DEBUG(printf("Add polygon (%d vertices) [%d, %d]:\n", vertices_count, floor_height, ceiling_height));

  const size_t pi = this->polygons_count;

  this->polygons[pi] = (polygon) {
    .vertices_count = vertices_count,
    .floor_height = floor_height,
    .ceiling_height = ceiling_height,
    .light = light
  };

  this->polygons[pi].vertices = (vec2f*)malloc(vertices_count * sizeof(vec2f));

  memcpy(this->polygons[pi].vertices, vertices, vertices_count * sizeof(vec2f));

  M_DEBUG(for (i = 0; i < vertices_count; ++i) {
    printf("\t(%d, %d)\n", XY(this->polygons[pi].vertices[i]));
  })

  this->polygons_count++;
}

level_data* map_builder_build(map_builder *this)
{
  int i;

  level_data *level = malloc(sizeof(level_data));
  level->sectors_count = 0;
  level->linedefs_count = 0;
  level->vertices_count = 0;

  M_DEBUG(printf("Building level (0x%p) ...\n", level));

  /* ------------ */
  
  M_DEBUG(printf("1. Find all polygon intersections ...\n"));
  
  map_builder_step_find_polygon_intersections(this, level);

  /* ------------ */
  
  M_DEBUG(printf("2. Creating sectors and linedefs (from %d polys) ...\n", this->polygons_count));

  for (i = 0; i < this->polygons_count; ++i) {
    level_data_create_sector_from_polygon(level, &this->polygons[i]);
  }

  /* ------------ */

  M_DEBUG(printf("3. Configure back sectors ...\n"));

  map_builder_step_configure_back_sectors(this, level);

  /* ------------ */

  M_DEBUG(printf("DONE!\n"));

  return level;
}

void map_builder_free(map_builder *this) {
  size_t i;
  for (i = 0; i < this->polygons_count; ++i) {
    free(this->polygons[i].vertices);
  }
}

/*
 * Private methods
 */

static void to_gpc_polygon(const polygon *poly, gpc_polygon *gpc_poly)
{
  size_t i;
  gpc_vertex_list contour;
  contour.num_vertices = poly->vertices_count;
  contour.vertex = (gpc_vertex*)malloc(poly->vertices_count * sizeof(gpc_vertex));
  for (i = 0; i < poly->vertices_count; ++i) {
    contour.vertex[i] = (gpc_vertex) { poly->vertices[i].x, poly->vertices[i].y };
  }
  gpc_add_contour(gpc_poly, &contour, 0);
  free(contour.vertex);
}

static void from_gpc_polygon(const gpc_polygon *gpc_poly, polygon *poly)
{
  size_t i, j;
  assert(gpc_poly->num_contours > 0);

  for (i = 0; i < gpc_poly->num_contours; ++i) {
    if (!gpc_poly->hole[i]) {
      poly->vertices_count = gpc_poly->contour[i].num_vertices;
      poly->vertices = realloc(poly->vertices, poly->vertices_count * sizeof(vec2f));
      for (j = 0; j < gpc_poly->contour[i].num_vertices; ++j) {
        poly->vertices[j] = VEC2F(gpc_poly->contour[i].vertex[j].x, gpc_poly->contour[i].vertex[j].y);
      }
    }
  }
}

static void polygon_add_new_vertices_from(polygon *this, const polygon *other)
{
  size_t j,i,i2;
  for (j = 0; j < other->vertices_count; ++j) {
    for (i = 0; i < this->vertices_count; ++i) {
      i2 = (i + 1) % this->vertices_count;
      if (math_point_on_line_segment(other->vertices[j], this->vertices[i], this->vertices[i2]) && !polygon_vertices_contains_point(this, other->vertices[j])) {
        M_DEBUG(printf("\t\t + Inserting (%d,%d) between (%d,%d) and (%d,%d)\n", XY(other->vertices[j]), XY(this->vertices[i]), XY(this->vertices[i2])));
        polygon_insert_point(this, other->vertices[j], this->vertices[i], this->vertices[i2]);
        break;
      }
    }
  }
}

static void map_builder_step_find_polygon_intersections(map_builder *this, level_data *level)
{
  int i, j;
  polygon *pi, *pj;

  for (j = 0; j < this->polygons_count; ++j) {
    pj = &this->polygons[j];

    for (i = j + 1; i < this->polygons_count; ++i) {
      pi = &this->polygons[i];

      /* Polygon 'pi' is wholly inside 'pj' without sharing an edge */
      if (polygon_contains_polygon(pj, pi, false) || !polygon_overlaps_polygon(pj, pi)) {
        continue;
      }

      M_DEBUG(printf("\tIntersect Sector %d with Sector %d\n", i, j));

      gpc_polygon subject = { 0 }, clip = { 0 }, result = { 0 };

      to_gpc_polygon(pj, &subject);
      to_gpc_polygon(pi, &clip);
      gpc_polygon_clip(GPC_DIFF, &subject, &clip, &result);

      from_gpc_polygon(&result, pj);

      gpc_free_polygon(&subject);
      gpc_free_polygon(&clip);
      gpc_free_polygon(&result);
    }
  }

  for (j = 0; j < this->polygons_count; ++j) {
    pj = &this->polygons[j];
    for (i = 0; i < this->polygons_count; ++i) {
      pi = &this->polygons[i];
      if (pi == pj) { continue; }
      /* Add new vertices from 'pj' that are on any of the edges of 'pi' */
      polygon_add_new_vertices_from(pi, pj);
    }
  }
}

static void map_builder_step_configure_back_sectors(map_builder *this, level_data *level)
{
  int i, j, k, new_count;
  sector *front, *back;
  linedef *line;

  for (j = level->sectors_count - 1; j >= 0; --j) {
    front = &level->sectors[j];

    for (i = j - 1; i >= 0; --i) {
      back = &level->sectors[i];
      
      if (back == front) { continue; }

      new_count = back->linedefs_count;

      for (k = 0; k < front->linedefs_count; ++k) {
        line = front->linedefs[k];

        if (line->side_sector[0] && line->side_sector[1]) { continue; }
        if (sector_connects_vertices(back, line->v0, line->v1)) { continue; }
        
        if (polygon_is_point_inside(&this->polygons[i], line->v0->point, false) && polygon_is_point_inside(&this->polygons[i], line->v1->point, false)) {
          M_DEBUG(printf("\t\tAdd contained line %d (%d,%d) <-> (%d,%d) of sector %d INTO sector %d\n", k, XY(line->v0->point), XY(line->v1->point), j, i));
          line->side_sector[1] = back;
          back->linedefs = realloc(back->linedefs, sizeof(linedef*) * (new_count+1));
          back->linedefs[new_count++] = line;
        } else if ((
             (sector_references_vertex(back, line->v0, 0) && polygon_is_point_inside(&this->polygons[i], line->v1->point, false))
          || (sector_references_vertex(back, line->v1, 0) && polygon_is_point_inside(&this->polygons[i], line->v0->point, false))
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
