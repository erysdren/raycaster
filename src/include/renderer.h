#ifndef RAYCAST_RENDERER_INCLUDED
#define RAYCAST_RENDERER_INCLUDED

#include "sector.h"
#include "camera.h"

typedef uint32_t pixel_type;
typedef pixel_type* frame_buffer;

#define RENDERER_DRAW_DISTANCE 10000.f

typedef struct {
  frame_buffer buffer;
  vec2u buffer_size;

  struct {
    uint32_t wall_pixels,
             wall_columns,
             ceiling_pixels,
             ceiling_columns,
             floor_pixels,
             floor_columns,
             line_checks;
  } counters;
} renderer;

void renderer_init(renderer *this, vec2u size);
void renderer_resize(renderer *this, vec2u new_size);
void renderer_destroy(renderer *this);
void renderer_draw(renderer *this, camera *camera);

#endif
