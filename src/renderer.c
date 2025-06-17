#include "renderer.h"
#include "maths.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

static uint32_t debug_colors[16][3] = {
  { 195, 235, 233 },
  { 123, 45, 67 },
  { 34, 200, 123 },
  { 87, 156, 231 },
  { 12, 89, 190 },
  { 250, 210, 100 },
  { 10, 240, 180 },
  { 76, 89, 240 },
  { 55, 200, 60 },
  { 190, 33, 90 },
  { 44, 66, 77 },
  { 100, 255, 200 },
  { 199, 123, 44 },
  { 155, 155, 155 },
  { 0, 123, 255 },
  { 231, 99, 178 }
};

static uint32_t debug_colors_dark[16][3] = {
  { 120, 110, 100 },
  { 123, 67, 45 },
  { 34, 100, 123 },
  { 87, 126, 111 },
  { 12, 89, 90 },
  { 128, 110, 100 },
  { 70, 120, 80 },
  { 76, 89, 120 },
  { 55, 100, 60 },
  { 90, 33, 90 },
  { 44, 66, 77 },
  { 100, 128, 100 },
  { 99, 123, 44 },
  { 88, 88, 88 },
  { 10, 123, 100 },
  { 111, 99, 78 }
};

typedef struct {
  struct {
    vec2f start,
          end;
  } ray;
  vec2f near_left,
        near_right,
        far_left,
        far_right;
  float unit_size, view_z;
  float top_limit, bottom_limit;
  uint32_t column, half_h;
  bool column_finished;
} frame_info;

typedef struct {
  vec2f point;
  float planar_distance,
        planar_distance_inv;
  linedef *line;
  sector* back_sector;
} line_hit;

static void check_sector_visibility(renderer*, frame_info *, sector *sect);
static void check_sector_column(renderer*, frame_info*, sector *sect, sector *prev_sect);
static void draw_wall_segment(renderer*, frame_info*, linedef *line, uint32_t from, uint32_t to);
static void draw_floor_segment(renderer*, frame_info*, sector *sect, uint32_t from, uint32_t to);
static void draw_ceiling_segment(renderer*, frame_info*, sector *sect, uint32_t from, uint32_t to);
static void draw_column(renderer*, frame_info*, sector *sect, sector *prev_sect, line_hit const *hit);

void renderer_init(
  renderer *this,
  vec2u size
) {
  this->buffer_size = size;
  this->buffer = malloc(size.x * size.y * sizeof(pixel_type));
}

void renderer_resize(
  renderer *this,
  vec2u new_size
) {
  this->buffer_size = new_size;
  this->buffer = realloc(this->buffer, new_size.x * new_size.y * sizeof(pixel_type));
}

void renderer_destroy(renderer *this) {
  if (this->buffer) {
    free(this->buffer);
    this->buffer = NULL;
  }
}

void renderer_draw(
  renderer *this,
  camera *camera
) {
  register uint32_t x;
  register float cam_x, rx, ry;
  frame_info info;

  assert(this->buffer);
  memset(this->buffer, 0, this->buffer_size.x * this->buffer_size.y * sizeof(pixel_type));
  
  this->tick++;

  info.near_left = vec2f_sub(camera->position, camera->plane),
  info.near_right = vec2f_add(camera->position, camera->plane);
  info.far_left = vec2f_add(camera->position, vec2f_mul(vec2f_sub(camera->direction, camera->plane), RENDERER_DRAW_DISTANCE));
  info.far_right = vec2f_add(camera->position, vec2f_mul(vec2f_add(camera->direction, camera->plane), RENDERER_DRAW_DISTANCE));
  info.ray.start = camera->position;
  info.half_h = this->buffer_size.y >> 1;
  info.unit_size = (this->buffer_size.x >> 1) / camera->fov;
  info.view_z = camera->z;

  this->counters.wall_pixels = 0;
  this->counters.wall_columns = 0;
  this->counters.ceiling_pixels = 0;
  this->counters.ceiling_columns = 0;
  this->counters.floor_pixels = 0;
  this->counters.floor_columns = 0;
  this->counters.line_checks = 0;
  this->counters.line_visibility_checks = 0;
  this->counters.visible_lines = 0;

  for (x = 0; x < this->buffer_size.x; ++x) {
    cam_x = ((x << 1) / (float)this->buffer_size.x) - 1;
    rx = camera->direction.x + (camera->plane.x * cam_x);
    ry = camera->direction.y + (camera->plane.y * cam_x);
    info.column = x;
    info.ray.end = vec2f_make(
      camera->position.x + (rx * RENDERER_DRAW_DISTANCE),
      camera->position.y + (ry * RENDERER_DRAW_DISTANCE)
    );

    info.top_limit = 0.f;
    info.bottom_limit = this->buffer_size.y-1;
    info.column_finished = false;

    check_sector_column(this, &info, camera->in_sector, NULL);
  }
}

/* ----- */

static void check_sector_visibility(
  renderer *this,
  frame_info *info,
  sector *sect
) {
  register size_t i;
  linedef *line;

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = sect->linedefs[i];

    if (line->last_visible_tick == this->tick) {
      continue;
    }

    this->counters.line_visibility_checks ++;

    if ( math_point_in_triangle(line->v0.point, info->ray.start, info->far_left, info->far_right)
      || math_point_in_triangle(line->v1.point, info->ray.start, info->far_left, info->far_right)
      || segmentsIntersect(line->v0.point, line->v1.point, info->ray.start, info->far_left)
      || segmentsIntersect(line->v0.point, line->v1.point, info->ray.start, info->far_right)) {
      this->counters.visible_lines ++;
      line->last_visible_tick = this->tick;
    }
  }
}

M_INLINED void sort_nearest(line_hit *arr, int n) {
  register int i, j;
  register line_hit hit;
  for (i = 1; i < n; ++i) {
    hit = arr[i];
    j = i-1;
    while (j >= 0 && arr[j].planar_distance > hit.planar_distance) {
      arr[j+1] = arr[j];
      j = j-1;
    }
    arr[j+1] = hit;
  }
}

static void check_sector_column(
  renderer *this,
  frame_info *info,
  sector *sect,
  sector *prev_sect
) {
  register size_t i, hits_count = 0;
  register float planar_distance;
  vec2f intersection;
  float intersectiond;
  linedef *line;
  line_hit hits[16];

  if (sect->last_visibility_check_tick != this->tick) {
    check_sector_visibility(this, info, sect);
  }

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = sect->linedefs[i];

    if (line->last_visible_tick != this->tick) {
      continue;
    }

    this->counters.line_checks ++;

    if (math_lines_intersect(line->v0.point, line->v1.point, info->ray.start, info->ray.end, &intersection, &intersectiond)) {
      // float point_distance = math_length(vec2f_sub(intersection, info->ray.start));
      planar_distance = math_line_segment_point_distance(info->near_left, info->near_right, intersection);
      hits[hits_count++] = (line_hit) {
        .point = intersection,
        .planar_distance = planar_distance,
        .planar_distance_inv = 1.f / planar_distance,
        .line = line,
        .back_sector = line->side_sector[0] == sect ? line->side_sector[1] : line->side_sector[0]
      };
    }
  }

  sort_nearest(hits, hits_count);

  for (i = 0; i < hits_count && !info->column_finished; ++i) {
    draw_column(this, info, sect, prev_sect, &hits[i]);
  }
}

static void draw_column(
  renderer *this,
  frame_info *info,
  sector *sect,
  sector *prev_sect,
  line_hit const *hit
) {
  register const float depth_scale_factor = (info->unit_size * hit->planar_distance_inv);
  register const float ceiling_z_scaled   = (sect->ceiling_height * depth_scale_factor);
  register const float floor_z_scaled     = (sect->floor_height * depth_scale_factor);
  register const float view_z_scaled      = (info->view_z * depth_scale_factor);

  sector *back_sector = hit->back_sector;

  if (!back_sector || (back_sector && back_sector->floor_height == back_sector->ceiling_height)) {
    /* Draw a full wall */
    float start_y = M_MAX(info->half_h - ceiling_z_scaled + view_z_scaled, info->top_limit);
    float end_y = M_CLAMP(info->half_h - floor_z_scaled + view_z_scaled, info->top_limit, info->bottom_limit);

    draw_wall_segment(this, info, hit->line, start_y, end_y);
    draw_ceiling_segment(this, info, sect, info->top_limit, M_CLAMP(start_y, info->top_limit, info->bottom_limit));
    draw_floor_segment(this, info, sect, M_MIN(end_y+1, info->bottom_limit+1), info->bottom_limit+1);

    info->column_finished = true;
  } else {
    /* Draw top and bottom segments of the wall and the sector behind */
    const float top_segment = M_MAX(sect->ceiling_height - back_sector->ceiling_height, 0) * depth_scale_factor;
    const float bottom_segment = M_MAX(back_sector->floor_height - sect->floor_height, 0) * depth_scale_factor;

    float top_start_y = M_CLAMP(info->half_h - ceiling_z_scaled + view_z_scaled, info->top_limit, info->bottom_limit);
    float top_end_y = M_CLAMP(info->half_h - ceiling_z_scaled + view_z_scaled + top_segment, info->top_limit, info->bottom_limit);
    float bottom_end_y = M_CLAMP(info->half_h - floor_z_scaled + view_z_scaled, info->top_limit, info->bottom_limit);
    float bottom_start_y = M_CLAMP(info->half_h - floor_z_scaled + view_z_scaled - bottom_segment, info->top_limit, info->bottom_limit);

    if (top_segment > 0) {
      draw_wall_segment(this, info, hit->line, top_start_y, top_end_y);
    }

    if (bottom_segment > 0) {
      draw_wall_segment(this, info, hit->line, bottom_start_y, bottom_end_y);
    }

    draw_ceiling_segment(this, info, sect, info->top_limit, M_MAX(top_start_y, info->top_limit));
    draw_floor_segment(this, info, sect, M_MIN(bottom_end_y+1, info->bottom_limit+1), info->bottom_limit+1);

    info->top_limit = top_end_y;
    info->bottom_limit = bottom_start_y;

    if ((int)info->top_limit == (int)info->bottom_limit) {
      info->column_finished = true;
      return;
    }
    
    /* Render back sector */
    if (back_sector != prev_sect) {
      check_sector_column(this, info, back_sector, sect);
    }
  }
}

static void draw_wall_segment(
  renderer *this,
  frame_info *info,
  linedef *line,
  uint32_t from,
  uint32_t to
) {
  if (from == to) {
    return;
  }

  this->counters.wall_pixels += (to-from);
  this->counters.wall_columns ++;

  register uint32_t y;
  register uint32_t *p = &this->buffer[from * this->buffer_size.x + info->column];
  register uint32_t *c = debug_colors[line->color % 16];

  for (y = from; y <= to; ++y, p += this->buffer_size.x) {
    *p = 0xFF000000 | (c[0] << 16) | (c[1] << 8) | c[2];
  }
}

static void draw_floor_segment(
  renderer *this,
  frame_info *info,
  sector *sect,
  uint32_t from,
  uint32_t to
) {
  /* Camera below the floor */
  if (from == to || info->view_z < sect->floor_height) {
    return;
  }

  this->counters.floor_pixels += (to-from);
  this->counters.floor_columns ++;

  register uint32_t y;
  register uint32_t *p = &this->buffer[from * this->buffer_size.x + info->column];
  register uint32_t *c = debug_colors_dark[sect->color % 16];

  for (y = from; y < to; ++y, p += this->buffer_size.x) {
    *p = 0xFF000000 | (c[0] << 16) | (c[1] << 8) | c[2];
  } 
}

static void draw_ceiling_segment(
  renderer *this,
  frame_info *info,
  sector *sect,
  uint32_t from,
  uint32_t to
) {
  /* Camera above the ceiling */
  if (from == to || info->view_z > sect->ceiling_height) {
    return;
  }

  this->counters.ceiling_pixels += (to-from);
  this->counters.ceiling_columns ++;

  register uint32_t y;
  register uint32_t *p = &this->buffer[from * this->buffer_size.x + info->column];
  register uint32_t *c = debug_colors_dark[sect->color % 16];

  for (y = from; y < to; ++y, p += this->buffer_size.x) {
    *p = 0xFF000000 | (c[0] << 16) | (c[1] << 8) | c[2];
  }
}
