#include "renderer.h"
#include "camera.h"
#include "level_data.h"
#include "maths.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifdef RAYCASTER_PARALLEL_RENDERING
  #include <omp.h>
#endif

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  #include <emmintrin.h>
  #include <xmmintrin.h>
#endif

#define MAX_SECTOR_HISTORY 64
#define MAX_LINE_HITS_PER_COLUMN 48

void (*texture_sampler)(texture_ref, float, float, texture_coordinates_func, uint8_t, uint8_t*, uint8_t*);

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
  #define INSERT_RENDER_BREAKPOINT if (renderer_step) { renderer_step(this); }
  void (*renderer_step)(const renderer*) = NULL;
#else
  #define INSERT_RENDER_BREAKPOINT
#endif

/* Common frame info all column renderers can share */
typedef struct {
  level_data *level;
  vec2f view_position,
        far_left,
        far_right;
  float unit_size, view_z;
  int32_t half_w, half_h, pitch_offset;
  texture_ref sky_texture;
} frame_info;

typedef struct ray_intersection {
  vec2f point;
  float planar_distance,
        planar_distance_inv,
        point_distance,
        point_distance_inverse,
        determinant;
  linedef *line;
  uint8_t side;
  uint8_t distance_steps;
#if !defined RAYCASTER_LIGHT_STEPS || (RAYCASTER_LIGHT_STEPS == 0)
  float light_falloff;
#endif
  struct ray_intersection *next;
} ray_intersection;

/* Column-specific data */
typedef struct {
  const sector *sector_history[MAX_SECTOR_HISTORY];
  struct {
    ray_intersection list[MAX_LINE_HITS_PER_COLUMN];
    ray_intersection *head;
    size_t count;
  } intersections;
  vec2f ray_start,
        ray_end,
        ray_direction,
        ray_direction_unit;
  float theta_inverse, top_limit, bottom_limit;
  uint32_t index, sector_depth, buffer_stride;
  pixel_type *buffer_start;
  bool finished;
} column_info;

#define DIMMING_DISTANCE 4096.f

#if RAYCASTER_LIGHT_STEPS > 0
// static const float LIGHT_STEP_DISTANCE = DIMMING_DISTANCE / RAYCASTER_LIGHT_STEPS;
static const float LIGHT_STEP_DISTANCE_INVERSE = 1.f / (DIMMING_DISTANCE / RAYCASTER_LIGHT_STEPS);
static const float LIGHT_STEP_VALUE_CHANGE = 1.f / RAYCASTER_LIGHT_STEPS;
static const float LIGHT_STEP_VALUE_CHANGE_INVERSE = 1.f / (1.f / RAYCASTER_LIGHT_STEPS);
#else
static const float LIGHT_STEP_DISTANCE_INVERSE = 1.f / (DIMMING_DISTANCE / 4);
static const float DIMMING_DISTANCE_INVERSE = 1.f / DIMMING_DISTANCE;
#endif

#ifdef RAYCASTER_PRERENDER_VISCHECK
  static void
  refresh_sector_visibility(renderer*, const frame_info*, sector*);
#endif

static void
find_sector_intersections(const renderer*, const frame_info*, column_info*, const sector*);

static void
draw_wall_segment(const renderer*, const frame_info*, column_info*, const sector*, const ray_intersection*, uint32_t from, uint32_t to, float, texture_ref);

static void
draw_floor_segment(const renderer*, const frame_info*, column_info*, const sector*, const ray_intersection*, float, uint32_t from, uint32_t to);

static void
draw_ceiling_segment(const renderer*, const frame_info*, column_info*, const sector*, const ray_intersection*, float, uint32_t from, uint32_t to);

static void
draw_column(const renderer*, const frame_info*, column_info*, const ray_intersection*);

static void
draw_sky_segment(const renderer *this, const frame_info*, const column_info*, uint32_t, uint32_t);

M_INLINED void init_depth_values(renderer *this) {
  register size_t y, h = this->buffer_size.y;
  this->depth_values = malloc(h*sizeof(float));
  for (y = 0; y < h; ++y) {
    this->depth_values[y] = 1.f / (y+1);
  }
}

void
renderer_init(
  renderer *this,
  vec2i size
) {
  this->buffer_size = size;
  this->buffer = malloc(size.x * size.y * sizeof(pixel_type));
  init_depth_values(this);
}

void
renderer_resize(
  renderer *this,
  vec2i new_size
) {
  this->buffer_size = new_size;
  this->buffer = realloc(this->buffer, new_size.x * new_size.y * sizeof(pixel_type));
  free((float*)this->depth_values);
  init_depth_values(this);
}

void
renderer_destroy(renderer *this)
{
  if (this->buffer) {
    free(this->buffer);
    this->buffer = NULL;
  }
}

void
renderer_draw(
  renderer *this,
  camera *camera
) {
  int32_t x;
  frame_info info;

  assert(this->buffer);
  memset(this->buffer, 0, this->buffer_size.x * this->buffer_size.y * sizeof(pixel_type));
  
  this->tick++;

  int32_t half_h = this->buffer_size.y >> 1;
  sector *root_sector = camera->entity.sector;

  info.level = camera->entity.level;
  info.view_position = camera->entity.position;
  info.far_left = vec2f_add(camera->entity.position, vec2f_mul(vec2f_sub(camera->entity.direction, camera->plane), RENDERER_DRAW_DISTANCE));
  info.far_right = vec2f_add(camera->entity.position, vec2f_mul(vec2f_add(camera->entity.direction, camera->plane), RENDERER_DRAW_DISTANCE));
  info.half_w = this->buffer_size.x >> 1;
  info.pitch_offset = (int32_t)floorf(camera->pitch * half_h);
  info.half_h = half_h + info.pitch_offset;
  info.unit_size = (this->buffer_size.x >> 1) / camera->fov;
  info.view_z = camera->entity.z;
  info.sky_texture = info.level->sky_texture;

#ifdef RAYCASTER_PRERENDER_VISCHECK
  refresh_sector_visibility(this, &info, root_sector);
#endif

#ifdef RAYCASTER_PARALLEL_RENDERING
  #pragma omp parallel for
#endif
  for (x = 0; x < this->buffer_size.x; ++x) {
    const float cam_x = ((x << 1) / (float)this->buffer_size.x) - 1;
    const vec2f ray = VEC2F(
      camera->entity.direction.x + (camera->plane.x * cam_x),
      camera->entity.direction.y + (camera->plane.y * cam_x)
    );
    const vec2f ray_end = VEC2F(
      camera->entity.position.x + (ray.x * RENDERER_DRAW_DISTANCE),
      camera->entity.position.y + (ray.y * RENDERER_DRAW_DISTANCE)
    );

    column_info column = (column_info) {
      .ray_start = camera->entity.position,
      .ray_end = ray_end,
      .ray_direction = vec2f_sub(ray_end, camera->entity.position),
      .ray_direction_unit = ray,
      .index = x,
      .intersections = { .count = 0, .head = NULL },
      .sector_depth = 0,
      .buffer_stride = this->buffer_size.x,
      .theta_inverse = 1.f / math_dot2(camera->entity.direction, ray),
      .top_limit = 0.f,
      .bottom_limit = this->buffer_size.y,
      .buffer_start = &this->buffer[x],
      .finished = false
    };

    find_sector_intersections(this, &info, &column, root_sector);
    draw_column(this, &info, &column, column.intersections.head);
  }

#if defined(RAYCASTER_DEBUG) && !defined(RAYCASTER_PARALLEL_RENDERING)
  renderer_step = NULL;
#endif
}

/* ----- */

#ifdef RAYCASTER_PRERENDER_VISCHECK

static void
refresh_sector_visibility(
  renderer *this,
  const frame_info *info,
  sector *sect
) {
  register size_t i;
  float sign;
  uint8_t side;
  linedef *line;
  sector *back_sector;

  sect->last_visibility_check_tick = this->tick;

  if (!sect->visible_linedefs) {
    sect->visible_linedefs = malloc(sect->linedefs_count * sizeof(linedef*));
  }
  sect->visible_linedefs_count = 0;

  for (i = 0; i < sect->linedefs_count; ++i) {
    line = sect->linedefs[i];
    side = line->side[0].sector == sect ? 0 : 1;
    sign = math_sign(line->v0->point, line->v1->point, info->view_position);

    if ((side == 0 && sign > 0) || (side == 1 && sign < 0)) {
      continue;
    }

    if (line->v0->last_visibility_check_tick != this->tick) {
      line->v0->last_visibility_check_tick = this->tick;
      line->v0->visible = math_point_in_triangle(line->v0->point, info->view_position, info->far_left, info->far_right);
    }

    if (line->v1->last_visibility_check_tick != this->tick) {
      line->v1->last_visibility_check_tick = this->tick;
      line->v1->visible = math_point_in_triangle(line->v1->point, info->view_position, info->far_left, info->far_right);
    }

    if (line->v0->visible || line->v1->visible
      || math_find_line_intersection(line->v0->point, line->v1->point, info->view_position, info->far_left, NULL, NULL)
      || math_find_line_intersection(line->v0->point, line->v1->point, info->view_position, info->far_right, NULL, NULL)) {
      sect->visible_linedefs[sect->visible_linedefs_count++] = line;
      back_sector = line->side[0].sector == sect ? line->side[1].sector : line->side[0].sector;

      if (back_sector && back_sector->last_visibility_check_tick != this->tick) {
        refresh_sector_visibility(this, info, back_sector);
      }
    }
  }
}

#endif

M_INLINED void
insert_sorted(ray_intersection *value, ray_intersection **head)
{
  if (!*head || value->planar_distance < (*head)->planar_distance) {
    value->next = *head;
    *head = value;
    return;
  }
  ray_intersection *cur = *head;
  while (cur->next && cur->next->planar_distance <= value->planar_distance) {
    cur = cur->next;
  }
  value->next = cur->next;
  cur->next = value;
}

static void
find_sector_intersections(
  const renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect
) {
  register size_t i;
  float planar_distance, point_distance;
  vec2f point;
  float line_det, ray_det;
  linedef *line;

  if (column->sector_depth == MAX_SECTOR_HISTORY) {
    return;
  }

  for (i = 0; i < column->sector_depth; ++i) {
    if (column->sector_history[i] == sect) {
      return;
    }
  }

  column->sector_history[column->sector_depth++] = sect;

  size_t insert_index;
  sector *back_sector;

#ifdef RAYCASTER_PRERENDER_VISCHECK
  for (i = 0; i < sect->visible_linedefs_count && column->intersections.count < MAX_LINE_HITS_PER_COLUMN; ++i) {
    line = sect->visible_linedefs[i];
#else
  for (i = 0; i < sect->linedefs_count && column->intersections.count < MAX_LINE_HITS_PER_COLUMN; ++i) {
    line = sect->linedefs[i];
#endif

    if (math_find_line_intersection_cached(line->v0->point, column->ray_start, line->direction, column->ray_direction, &point, &line_det, &ray_det)) {
      planar_distance = ray_det * RENDERER_DRAW_DISTANCE;
      point_distance = planar_distance * column->theta_inverse;

      insert_index = column->intersections.count++;

      column->intersections.list[insert_index] = (ray_intersection) {
        .point = point,
        .planar_distance = planar_distance,
        .planar_distance_inv = 1.f / planar_distance,
        .point_distance = point_distance,
        .point_distance_inverse = 1.f / point_distance,
        .determinant = line_det,
        .line = line,
        .side = line->side[0].sector == sect ? 0 : 1,
        .distance_steps = (uint8_t)(point_distance * LIGHT_STEP_DISTANCE_INVERSE),
#if !defined RAYCASTER_LIGHT_STEPS || (RAYCASTER_LIGHT_STEPS == 0)
        .light_falloff = point_distance * DIMMING_DISTANCE_INVERSE,
#endif
        .next = NULL
      };

      insert_sorted(
        &column->intersections.list[insert_index],
        &column->intersections.head
      );

      if ((back_sector = line->side[0].sector == sect ? line->side[1].sector : line->side[0].sector)) {
        find_sector_intersections(this, info, column, back_sector);
      }
    }
  }
}

static void
draw_column(
  const renderer *this,
  const frame_info *info,
  column_info *column,
  const ray_intersection *intersection
) {
  if (!intersection) { return; }

  const struct linedef_side *front_side = &intersection->line->side[intersection->side];

  const sector *sect              = front_side->sector;
  const sector *back_sector       = intersection->line->side[!intersection->side].sector;
  const float depth_scale_factor  = info->unit_size * intersection->planar_distance_inv;
  const float ceiling_z_scaled    = sect->ceiling.height * depth_scale_factor;
  const float floor_z_scaled      = sect->floor.height * depth_scale_factor;
  const float view_z_scaled       = info->view_z * depth_scale_factor;
  const float ceiling_z_local     = info->half_h - ceiling_z_scaled + view_z_scaled;
  const float floor_z_local       = info->half_h - floor_z_scaled + view_z_scaled;

  if (!back_sector) {
    /* Draw a full wall */
    const float start_y = ceilf(M_MAX(ceiling_z_local, column->top_limit));
    const float end_y = M_CLAMP(floor_z_local, column->top_limit, column->bottom_limit);

    draw_wall_segment(
      this,
      info,
      column,
      sect,
      intersection,
      start_y,
      end_y,
      view_z_scaled,
      front_side->texture[LINE_TEXTURE_MIDDLE]
    );

    if (sect->ceiling.texture != TEXTURE_NONE) {
      draw_ceiling_segment(
        this,
        info,
        column,
        sect,
        intersection,
        (sect->ceiling.height - info->view_z) * info->unit_size,
        column->top_limit,
        M_MIN(start_y, column->bottom_limit)
      );
    } else {
      draw_sky_segment(this, info, column, column->top_limit, M_MIN(start_y, column->bottom_limit));
    }

    draw_floor_segment(
      this,
      info,
      column,
      sect,
      intersection,
      (info->view_z - sect->floor.height) * info->unit_size,
      end_y,
      column->bottom_limit
    );

    column->finished = true;
  } else {
    /* Draw top and bottom segments of the wall and the sector behind */
    const float top_segment = (sect->ceiling.height - back_sector->ceiling.height) * depth_scale_factor;
    const float bottom_segment = (back_sector->floor.height - sect->floor.height) * depth_scale_factor;

    const float top_start_y = ceilf(math_clamp(ceiling_z_local, column->top_limit, column->bottom_limit));
    const float top_end_y = ceilf(math_clamp(ceiling_z_local + top_segment, column->top_limit, column->bottom_limit));
    const float bottom_end_y = math_clamp(floor_z_local, column->top_limit, column->bottom_limit);
    const float bottom_start_y = math_clamp(floor_z_local - bottom_segment, column->top_limit, column->bottom_limit);

    const bool back_sector_has_sky = back_sector->ceiling.texture == TEXTURE_NONE;

    float new_top_limit = column->top_limit;
    float new_bottom_limit = column->bottom_limit;

    if (!back_sector_has_sky) {
      if (top_segment > 0) {
        draw_wall_segment(
          this,
          info,
          column,
          sect,
          intersection,
          top_start_y,
          top_end_y,
          view_z_scaled,
          front_side->texture[LINE_TEXTURE_TOP]
        );
        new_top_limit = top_end_y;
      } else {
        new_top_limit = top_start_y;
      }
    }

    if (bottom_segment > 0) {
      draw_wall_segment(
        this,
        info,
        column,
        sect,
        intersection,
        bottom_start_y,
        bottom_end_y,
        view_z_scaled,
        front_side->texture[LINE_TEXTURE_BOTTOM]
      );
      new_bottom_limit = bottom_start_y;
    } else {
      new_bottom_limit = bottom_end_y;
    }

    if (sect->ceiling.texture != TEXTURE_NONE) {
      draw_ceiling_segment(
        this,
        info,
        column,
        sect,
        intersection,
        (sect->ceiling.height - info->view_z) * info->unit_size,
        column->top_limit,
        top_start_y
      );
      if (back_sector_has_sky) {
        new_top_limit = top_start_y;
      }
    } else {
      draw_sky_segment(this, info, column, column->top_limit, M_MAX(top_start_y, column->top_limit));
    }
      
    draw_floor_segment(
      this,
      info,
      column,
      sect,
      intersection,
      (info->view_z - sect->floor.height) * info->unit_size,
      bottom_end_y,
      column->bottom_limit
    );

    column->top_limit = new_top_limit;
    column->bottom_limit = new_bottom_limit;

    if ((int)column->top_limit == (int)column->bottom_limit || back_sector->floor.height == back_sector->ceiling.height) {
      column->finished = true;
      return;
    }

    /* Render next ray intersection */
    draw_column(this, info, column, intersection->next);

    /* Draw transparent middle texture from back to front */
    if (front_side->texture[LINE_TEXTURE_MIDDLE] != TEXTURE_NONE) {
      draw_wall_segment(
        this,
        info,
        column,
        sect,
        intersection,
        new_top_limit,
        new_bottom_limit,
        view_z_scaled,
        front_side->texture[LINE_TEXTURE_MIDDLE]
      );
    }
  }
}

/*
 * There are three light functions here:
 * 
 * When a surface is affected by a dynamic light:
 *   1. For horizontal surfaces (floors, ceilings) with a little falloff/attenuation
 *      as the light approaches and goes below it
 *   2. For vertical surfaces (walls)
 * 
 * When it's not:
 *   3. Basic brightness and dimming
 */

#define VERTICAL_FADE_DIST 2.5f

M_INLINED float
calculate_horizontal_surface_light(const sector *sect, vec3f pos, bool is_floor, size_t num_lights, light **lights,
#if RAYCASTER_LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  size_t i;
  vec3f world_pos;
  light *lt;
  float dz, v = sect->brightness, dsq;

  for (i = 0; i < num_lights; ++i) {
    lt = lights[i];

    /* Too far off the floor or ceiling */
    if ((dz = is_floor ? (lt->entity.z - sect->floor.height) : (sect->ceiling.height - lt->entity.z)) && (dz < 0.f)) {
      continue;
    }

    world_pos = entity_world_position(&lt->entity);

    if ((dsq = math_vec3_distance_squared(pos, world_pos)) > lt->radius_sq) {
      continue;
    }

#ifdef RAYCASTER_DYNAMIC_SHADOWS
    v = !map_cache_intersect_3d(&lt->entity.level->cache, pos, world_pos)
      ? math_max(v, lt->strength * math_min(1.f, dz / VERTICAL_FADE_DIST) * (1.f - (dsq * lt->radius_sq_inverse)))
      : v;
#else
    v = math_max(v, lt->strength * math_min(1.f, dz / VERTICAL_FADE_DIST) * (1.f - (dsq * lt->radius_sq_inverse)));
#endif
  }

  return math_max(
    0.f,
#if RAYCASTER_LIGHT_STEPS > 0
    ((uint8_t)(v * LIGHT_STEP_VALUE_CHANGE_INVERSE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE)
#else
    v - light_falloff
#endif
  );
}


M_INLINED float
calculate_vertical_surface_light(const sector *sect, vec3f pos, size_t num_lights, light **lights,
#if RAYCASTER_LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  size_t i;
  light *lt;
  vec3f world_pos;
  float v = sect->brightness, dsq;

  for (i = 0; i < num_lights; ++i) {
    lt = lights[i];
    world_pos = entity_world_position(&lt->entity);

    if ((dsq = math_vec3_distance_squared(pos, world_pos)) > lt->radius_sq) {
      continue;
    }

#ifdef RAYCASTER_DYNAMIC_SHADOWS
    v = !map_cache_intersect_3d(&lt->entity.level->cache, pos, world_pos)
      ? math_max(v, lt->strength * (1.f - (dsq * lt->radius_sq_inverse)))
      : v;
#else
    v = math_max(v, lt->strength * (1.f - (dsq * lt->radius_sq_inverse)));
#endif
  }

  return math_max(
    0.f,
#if RAYCASTER_LIGHT_STEPS > 0
    ((uint8_t)(v * LIGHT_STEP_VALUE_CHANGE_INVERSE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE)
#else
    v - light_falloff
#endif
  );
}

M_INLINED float
calculate_basic_brightness(const float base,
#if RAYCASTER_LIGHT_STEPS > 0
  uint8_t steps
#else
  float light_falloff
#endif
) {
  return math_max(
    0.f,
#if RAYCASTER_LIGHT_STEPS > 0
    ((uint8_t)(base * LIGHT_STEP_VALUE_CHANGE_INVERSE) * LIGHT_STEP_VALUE_CHANGE) - (steps * LIGHT_STEP_VALUE_CHANGE)
#else
    base - light_falloff
#endif
  );
}

static void
draw_wall_segment(
  const renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect,
  const ray_intersection *intersection,
  uint32_t from,
  uint32_t to,
  float view_z_scaled,
  texture_ref texture
) {
  if (from >= to || texture == TEXTURE_NONE) {
    return;
  }

  register uint32_t y;
  const float texture_step  = intersection->planar_distance / info->unit_size;
  const float texture_x     = intersection->determinant * intersection->line->length;
  const uint16_t segment    = (uint16_t)floorf((intersection->line->segments - 1) * intersection->determinant);
  uint32_t *p               = column->buffer_start + (from*column->buffer_stride);
  uint8_t rgb[3];
  uint8_t mask;
  uint8_t lights_count      = intersection->line->side[intersection->side].segments[segment].lights_count;
  struct light **lights     = intersection->line->side[intersection->side].segments[segment].lights;
  register float light      = !lights_count ? calculate_basic_brightness(
      sect->brightness,
#if RAYCASTER_LIGHT_STEPS > 0
      intersection->distance_steps
#else
      intersection->light_falloff
#endif
  ) : 0.f, texture_y        = (((float)from - info->half_h - view_z_scaled /*+ floor_z_scaled*/) * texture_step);

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  int32_t temp[4];
#endif

  for (y = from; y < to; ++y, p += column->buffer_stride, texture_y += texture_step) {
    texture_sampler(texture, texture_x, texture_y, &texture_coordinates_scaled, 1 + intersection->distance_steps, &rgb[0], &mask);
 
    if (!mask) { continue; } /* Transparent */

    light = lights_count ?
      calculate_vertical_surface_light(
        sect,
        VEC3F(intersection->point.x, intersection->point.y, -texture_y),
        lights_count,
        lights,
#if RAYCASTER_LIGHT_STEPS > 0
        intersection->distance_steps
#else
        intersection->light_falloff
#endif
      ) : light;

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
    _mm_storeu_si128((__m128i*)temp, _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, rgb[2], rgb[1], rgb[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f))));
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    *p = 0xFF000000|((uint8_t)math_min((rgb[0]*light),255)<<16)|((uint8_t)math_min((rgb[1]*light),255)<<8)|(uint8_t)math_min((rgb[2]*light),255);
#endif

    INSERT_RENDER_BREAKPOINT
  }
}

static void
draw_floor_segment(
  const renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect,
  const ray_intersection *intersection,
  float distance_from_view,
  uint32_t from,
  uint32_t to
) {
  /* Camera below the floor */
  if (from >= to || info->view_z < sect->floor.height || sect->floor.texture == TEXTURE_NONE) {
    return;
  }

  register uint32_t y, yz;
  register float light=-1, distance, weight, wx, wy;
  uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  uint8_t rgb[3], lights_count;
  map_cache_cell *cell;

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  int32_t temp[4];
#endif

  for (y = from, yz = from - info->half_h; y < to; ++y, p += column->buffer_stride) {
    distance = (distance_from_view * this->depth_values[yz++]) * column->theta_inverse;
    weight = math_min(1.f, distance * intersection->point_distance_inverse);
    wx = (weight * intersection->point.x) + ((1-weight) * column->ray_start.x);
    wy = (weight * intersection->point.y) + ((1-weight) * column->ray_start.y);
    cell = map_cache_cell_at(&info->level->cache, VEC2F(wx, wy));
    lights_count = cell ? cell->lights_count : 0;

    texture_sampler(sect->floor.texture, wx, wy, &texture_coordinates_scaled, 1 + (uint8_t)(distance * LIGHT_STEP_DISTANCE_INVERSE), &rgb[0], NULL);

    light = lights_count ? calculate_horizontal_surface_light(
      sect,
      VEC3F(wx, wy, sect->floor.height),
      true,
      lights_count,
      cell ? cell->lights : NULL,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    ) : calculate_basic_brightness(
      sect->brightness,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    );

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
    _mm_storeu_si128((__m128i*)temp, _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, rgb[2], rgb[1], rgb[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f))));
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    *p = 0xFF000000|((uint8_t)math_min((rgb[0]*light),255)<<16)|((uint8_t)math_min((rgb[1]*light),255)<<8)|(uint8_t)math_min((rgb[2]*light),255);
#endif

    INSERT_RENDER_BREAKPOINT
  } 
}

static void
draw_ceiling_segment(
  const renderer *this,
  const frame_info *info,
  column_info *column,
  const sector *sect,
  const ray_intersection *intersection,
  float distance_from_view,
  uint32_t from,
  uint32_t to
) {
  /* Camera above the ceiling */
  if (from >= to || info->view_z > sect->ceiling.height) {
    return;
  }

  register uint32_t y, yz;
  register float light=-1, distance, weight, wx, wy;
  uint32_t *p = column->buffer_start + (from*column->buffer_stride);
  uint8_t rgb[3], lights_count;
  map_cache_cell *cell;

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
  int32_t temp[4];
#endif

  for (y = from, yz = info->half_h - from - 1; y < to; ++y, p += column->buffer_stride) {
    distance = (distance_from_view * this->depth_values[yz--]) * column->theta_inverse;
    weight = math_min(1.f, distance * intersection->point_distance_inverse);
    wx = (weight * intersection->point.x) + ((1-weight) * column->ray_start.x);
    wy = (weight * intersection->point.y) + ((1-weight) * column->ray_start.y);
    cell = map_cache_cell_at(&info->level->cache, VEC2F(wx, wy));
    lights_count = cell ? cell->lights_count : 0;

    texture_sampler(sect->ceiling.texture, wx, wy, &texture_coordinates_scaled, 1 + (uint8_t)(distance * LIGHT_STEP_DISTANCE_INVERSE), &rgb[0], NULL);

    light = lights_count ? calculate_horizontal_surface_light(
      sect,
      VEC3F(wx, wy, sect->ceiling.height),
      false,
      lights_count,
      cell ? cell->lights : NULL,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    ) : calculate_basic_brightness(
      sect->brightness,
#if RAYCASTER_LIGHT_STEPS > 0
      distance * LIGHT_STEP_DISTANCE_INVERSE
#else
      distance * DIMMING_DISTANCE_INVERSE
#endif
    );

#ifdef RAYCASTER_SIMD_PIXEL_LIGHTING
    _mm_storeu_si128((__m128i*)temp, _mm_cvtps_epi32(_mm_min_ps(_mm_mul_ps(_mm_set_ps(0, rgb[2], rgb[1], rgb[0]), _mm_set1_ps(light)), _mm_set1_ps(255.0f))));
    *p = 0xFF000000 | (temp[0] << 16) | (temp[1] << 8) | temp[2];
#else
    *p = 0xFF000000|((uint8_t)math_min((rgb[0]*light),255)<<16)|((uint8_t)math_min((rgb[1]*light),255)<<8)|(uint8_t)math_min((rgb[2]*light),255);
#endif

    INSERT_RENDER_BREAKPOINT
  }
}

static void
draw_sky_segment(const renderer *this, const frame_info *info, const column_info *column, uint32_t from, uint32_t to)
{
  if (from == to || info->sky_texture == TEXTURE_NONE) {
    return;
  }

  register uint16_t y;
  uint8_t rgb[3];
  float angle = atan2f(column->ray_direction_unit.x, column->ray_direction_unit.y) * (180.0f / M_PI);
  if (angle < 0.0f) {
    angle += 360.0f;
  }
  float sky_x = angle / 360, h = (float)this->buffer_size.y; 
  uint32_t *p = column->buffer_start + (from * column->buffer_stride);

  for (y = from; y < to; ++y, p += column->buffer_stride) {
    texture_sampler(info->sky_texture, sky_x, math_min(1.f, 0.5f+(y-info->pitch_offset)/h), &texture_coordinates_normalized, 1, &rgb[0], NULL);
    *p = 0xFF000000 | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
    INSERT_RENDER_BREAKPOINT
  }
}
