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
  vec2f near_left;
  vec2f near_right;
  float unit_size, view_z;
  uint32_t column, half_h;
} frame_info;

static void check_sector_column(renderer*, frame_info*, sector *sect, sector *prev_sect);
static void draw_wall_segment(renderer*, frame_info*, linedef *line, uint32_t from, uint32_t to);
static void draw_floor_segment(renderer*, frame_info*, sector *sect, uint32_t from);
static void draw_ceiling_segment(renderer*, frame_info*, sector *sect, uint32_t to);
static void draw_column(renderer*, frame_info*, sector *sect, sector *prev_sect, linedef *line, float planar_distance, float planar_distance_inv);

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

  info.near_left = vec2f_sub(camera->position, camera->plane),
  info.near_right = vec2f_add(camera->position, camera->plane);
  info.ray.start = camera->position;
  info.half_h = this->buffer_size.y >> 1;
  info.unit_size = (this->buffer_size.x >> 1) / camera->fov;
  info.view_z = camera->z;

  for (x = 0; x < this->buffer_size.x; ++x) {
    cam_x = ((x << 1) / (float)this->buffer_size.x) - 1;
    rx = camera->direction.x + (camera->plane.x * cam_x);
    ry = camera->direction.y + (camera->plane.y * cam_x);
    info.column = x;
    info.ray.end = vec2f_make(
      camera->position.x + (rx * RENDERER_DRAW_DISTANCE),
      camera->position.y + (ry * RENDERER_DRAW_DISTANCE)
    );

    check_sector_column(this, &info, camera->in_sector, NULL);
  }
}

/* ----- */

static void check_sector_column(
  renderer *this,
  frame_info *info,
  sector *sect,
  sector *prev_sect
) {
  register size_t i;
  register float planar_distance, planar_distance_inv;
  vec2f intersection;
  float intersectiond;
  linedef *line;

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = &sect->linedefs[i];

    if (prev_sect && line->side_sector[LINEDEF_BACK] == prev_sect) {
      continue;
    }

    if (math_lines_intersect(line->v0.point, line->v1.point, info->ray.start, info->ray.end, &intersection, &intersectiond)) {
      // float point_distance = math_length(vec2f_sub(intersection, info->ray.start));
      planar_distance = math_line_segment_point_distance(info->near_left, info->near_right, intersection);
      planar_distance_inv = (1.f / planar_distance);

      draw_column(this, info, sect, prev_sect, line, planar_distance, planar_distance_inv);
    }
  }
}

static void draw_column(
  renderer *this,
  frame_info *info,
  sector *sect,
  sector *prev_sect,
  linedef *line,
  float planar_distance,
  float planar_distance_inv
) {
  register const float depth_scale_factor = (info->unit_size * planar_distance_inv);
  register const float ceiling_z_scaled   = (sect->ceiling_height * depth_scale_factor);
  register const float floor_z_scaled     = (sect->floor_height * depth_scale_factor);
  register const float view_z_scaled      = (info->view_z * depth_scale_factor);

  sector *back_sector = line->side_sector[LINEDEF_BACK];

  if (!back_sector || (back_sector && back_sector->floor_height == back_sector->ceiling_height)) {
    /* Draw a full wall */
    float start_y = info->half_h - ceiling_z_scaled + view_z_scaled;
    float end_y = info->half_h - floor_z_scaled + view_z_scaled;

    draw_wall_segment(this, info, line, M_MAX(start_y, 0), M_MIN(M_MAX(end_y, 0), this->buffer_size.y - 1));
    draw_ceiling_segment(this, info, sect, M_MAX(start_y - 1, 0));
    draw_floor_segment(this, info, sect, M_MIN(end_y + 1, this->buffer_size.y - 1));
  } else {
    /* Draw top and bottom segments of the wall and the sector behind */
    const float top_segment = M_MAX(sect->ceiling_height - back_sector->ceiling_height, 0) * depth_scale_factor;
    const float bottom_segment = M_MAX(back_sector->floor_height - sect->floor_height, 0) * depth_scale_factor;

    float top_start_y = info->half_h - ceiling_z_scaled + view_z_scaled;
    float top_end_y = top_start_y + top_segment;
    float bottom_end_y = info->half_h - floor_z_scaled + view_z_scaled;
    float bottom_start_y = bottom_end_y - bottom_segment;

    if (top_segment > 0) {
      draw_wall_segment(this, info, line, M_MAX(top_start_y, 0), M_MIN(M_MAX(top_end_y, 0), this->buffer_size.y - 1));
    }

    if (bottom_segment > 0) {
      draw_wall_segment(this, info, line, M_MAX(bottom_start_y, 0), M_MIN(M_MAX(bottom_end_y, 0), this->buffer_size.y - 1));
    }

    draw_ceiling_segment(this, info, sect, M_MAX(top_start_y - 1, 0));
    draw_floor_segment(this, info, sect, M_MIN(bottom_end_y + 1, this->buffer_size.y - 1));

    /* Render back sector */
    check_sector_column(this, info, back_sector, sect);
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

  register uint32_t y;
  register uint32_t *p = &this->buffer[from * this->buffer_size.x + info->column];
  register uint32_t c = line->color % 16;
  register bool started = false;

  for (y = from; y <= to; ++y, p += this->buffer_size.x) {
    if (!*p) {
      started = true;
      *p = 0xFF000000
        | (debug_colors[c][0] << 16)
        | (debug_colors[c][1] << 8)
        | debug_colors[c][2];
    } else if (started) {
      break;
    }
  }
}

static void draw_floor_segment(
  renderer *this,
  frame_info *info,
  sector *sect,
  uint32_t from
) {
  /* Camera below the floor */
  if (from == this->buffer_size.y - 1 || info->view_z < sect->floor_height) {
    return;
  }

  register uint32_t y;
  register uint32_t *p = &this->buffer[from * this->buffer_size.x + info->column];
  register uint32_t c = sect->color % 16;

  for (y = from; y < this->buffer_size.y; ++y, p += this->buffer_size.x) {
    if (!*p) {
      *p = 0xFF000000
        | (debug_colors_dark[c][0] << 16)
        | (debug_colors_dark[c][1] << 8)
        | debug_colors_dark[c][2];
    } else {
     break;
    }
  } 
}

static void draw_ceiling_segment(
  renderer *this,
  frame_info *info,
  sector *sect,
  uint32_t from
) {
  /* Camera above the ceiling */
  if (!from || info->view_z > sect->ceiling_height) {
    return;
  }

  register int32_t y;
  register uint32_t *p = &this->buffer[from * this->buffer_size.x + info->column];
  register uint32_t c = sect->color % 16;

  for (y = from; y >= 0; --y, p -= this->buffer_size.x) {
    if (!*p) {
      *p = 0xFF000000
        | (debug_colors_dark[c][0] << 16)
        | (debug_colors_dark[c][1] << 8)
        | debug_colors_dark[c][2];
    } else {
      break;
    }
  }
}
