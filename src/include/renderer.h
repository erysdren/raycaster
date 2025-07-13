#ifndef RAYCAST_RENDERER_INCLUDED
#define RAYCAST_RENDERER_INCLUDED

#include "sector.h"
#include "camera.h"

typedef uint32_t pixel_type;
typedef pixel_type* frame_buffer;

#define RENDERER_DRAW_DISTANCE 12000.f

typedef struct {
  volatile frame_buffer buffer;
  volatile float *depth_values;
  vec2i buffer_size;
  uint32_t tick;
} renderer;

void renderer_init(renderer *this, vec2i size);
void renderer_resize(renderer *this, vec2i new_size);
void renderer_destroy(renderer *this);
void renderer_draw(renderer *this, camera *camera);

IF_DEBUG(extern void (*renderer_step)(const renderer*))

#endif
