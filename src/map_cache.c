#include "map_cache.h"
#include "level_data.h"
#include <time.h>

#define CELL_SIZE 76.f

void
map_cache_process_level_data(map_cache *this, level_data *data)
{
  register size_t i, x, y;
  const int16_t cells_w = math_max(1, ceilf((data->max.x - data->min.x) / CELL_SIZE));
  const int16_t cells_h = math_max(1, ceilf((data->max.y - data->min.y) / CELL_SIZE));
  vec2f v0, v1, p0, p1, p2, p3;
  linedef *line;
  cache_cell *cell;

  M_DEBUG(printf(
    "\tLevel bounds:\n"
    "\t\tMin: %f, %f\n"
    "\t\tMax: %f, %f\n"
    "\tHorizontal cells: %d\n"
    "\tVertical cells: %d\n",
    data->min.x, data->min.y,
    data->max.x, data->max.y,
    cells_w,
    cells_h
  ); clock_t begin = clock());

  this->w = cells_w;
  this->h = cells_h;
  this->origin = data->min;
  this->cells = (cache_cell*)malloc(sizeof(cache_cell)*cells_w*cells_h);

  for (y = 0; y < cells_h; ++y) {
    for (x = 0; x < cells_w; ++x) {
      cell = &this->cells[y*cells_w + x];
      cell->count = 0;
      p0 = VEC2F(x*CELL_SIZE, y*CELL_SIZE);
      p1 = VEC2F(x*CELL_SIZE+CELL_SIZE, y*CELL_SIZE);
      p2 = VEC2F(x*CELL_SIZE+CELL_SIZE, y*CELL_SIZE+CELL_SIZE);
      p3 = VEC2F(x*CELL_SIZE, y*CELL_SIZE+CELL_SIZE);

      for (i = 0; i < data->linedefs_count; ++i) {
        line = &data->linedefs[i];

        v0 = vec2f_sub(line->v0->point, this->origin);
        v1 = vec2f_sub(line->v1->point, this->origin);

        if (math_find_line_intersection(v0, v1, p0, p1, NULL, NULL) ||
            math_find_line_intersection(v0, v1, p1, p2, NULL, NULL) ||
            math_find_line_intersection(v0, v1, p2, p3, NULL, NULL) ||
            math_find_line_intersection(v0, v1, p3, p0, NULL, NULL) ||
            ((v0.x >= p0.x && v0.y >= p0.y && v0.x < p2.x && v0.y < p2.y) &&
             (v1.x >= p0.x && v1.y >= p0.y && v1.x < p2.x && v1.y < p2.y))
        ) {
          if (cell->count == 0) {
            cell->linedefs = (linedef**)malloc(sizeof(linedef*));
          } else {
            cell->linedefs = (linedef**)realloc(cell->linedefs, (cell->count+1)*sizeof(linedef*));
          }
          cell->linedefs[cell->count++] = line;
        }
      }

      // printf("Cell %d, %d references %d lines\n", x, y, cell->count);
    }
  }

  M_DEBUG(printf("Time taken: %.3fs\n", (double)(clock() - begin) / CLOCKS_PER_SEC));
}

M_INLINED bool
collide(const map_cache *this, int x, int y, float current_z, float next_z, float dz, vec3f start, vec3f end)
{
  const cache_cell *cell = &this->cells[y*this->w+x];

  if (cell->count == 0) {
    return false;
  }

  const vec2f start_xy = VEC2F(start.x, start.y);
  const vec2f end_xy = VEC2F(end.x, end.y);
  register size_t li;
  float det, z;
  linedef *line;

  for (li = 0; li < cell->count; ++li) {
    line = cell->linedefs[li];

    if (dz < 0) {
      if (line->max_floor_height < next_z && line->min_ceiling_height > current_z) {
        continue;
      }
    } else {
      if (line->max_floor_height < current_z && line->min_ceiling_height > next_z) {
        continue;
      }
    }

    if (math_find_line_intersection(start_xy, end_xy, line->v0->point, line->v1->point, NULL, &det) && det > MATHS_EPSILON) {
      if (!line->side[1].sector) {
        return true;
      }

      z = start.z + dz*det;

      if (z < line->max_floor_height || z > line->min_ceiling_height) {
        return true;
      }
    }
  }

  return false;
}

bool
map_cache_intersect_3d(const map_cache *this, vec3f _start, vec3f _end)
{
  float dx = _end.x - _start.x;
  float dy = _end.y - _start.y;
  float dz = _end.z - _start.z;
  float fdx = 1.f / fabsf(dx);
  float fdy = 1.f / fabsf(dy);

  vec2f start = vec2f_sub(VEC2F(_start.x, _start.y), this->origin);
  vec2f end = vec2f_sub(VEC2F(_end.x, _end.y), this->origin);

  start.x += (dx < 0) ? -0.001f : (dx > 0) ? 0.001f : 0.f;
  start.y += (dy < 0) ? -0.001f : (dy > 0) ? 0.001f : 0.f;
  end.x += (dx < 0) ? -0.001f : (dx > 0) ? 0.001f : 0.f;
  end.y += (dy < 0) ? -0.001f : (dy > 0) ? 0.001f : 0.f;

  int ix = (int)floorf(start.x / CELL_SIZE);
  int iy = (int)floorf(start.y / CELL_SIZE);

  const int ix_end = (int)floorf(end.x / CELL_SIZE);
  const int iy_end = (int)floorf(end.y / CELL_SIZE);
  const int step_x = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
  const int step_y = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
  const float tDeltaX = (step_x != 0) ? CELL_SIZE * fdx : INFINITY;
  const float tDeltaY = (step_y != 0) ? CELL_SIZE * fdy : INFINITY;
  const float x_offset = (step_x > 0) ? (CELL_SIZE * (ix + 1) - start.x) : (start.x - CELL_SIZE * ix);
  const float y_offset = (step_y > 0) ? (CELL_SIZE * (iy + 1) - start.y) : (start.y - CELL_SIZE * iy);
  register float tMaxX = (step_x != 0) ? x_offset * fdx : INFINITY;
  register float tMaxY = (step_y != 0) ? y_offset * fdy : INFINITY;
  register float t = 0.f;

  // printf("Go from (%f, %f) %d, %d to (%f, %f) %d, %d:\n", start.x, start.y, ix, iy, end.x, end.y, ix_end, iy_end);

  while (1) {
    if (collide(this, ix, iy, _start.z + t * dz, _start.z + ((tMaxX < tMaxY) ? tMaxX : tMaxY) * dz, dz, _start, _end)) {
      return true;
    }

    if (ix == ix_end && iy == iy_end) {
      return false;
    }

    if (tMaxX < tMaxY) {
      t = tMaxX;
      tMaxX += tDeltaX;
      ix += step_x;
    } else {
      t = tMaxY;
      tMaxY += tDeltaY;
      iy += step_y;
    }
  }

  return false;
}
