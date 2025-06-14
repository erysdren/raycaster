#ifndef RAYCAST_CAMERA_INCLUDED
#define RAYCAST_CAMERA_INCLUDED

#include "types.h"
#include "level_data.h"

typedef struct {
  vec2f position,
        direction,
        plane;
  float fov, z;
  const level_data *level;
  sector *in_sector;
} camera;

void camera_init(camera *this, level_data *level);
void camera_move(camera *this, float movement);
void camera_rotate(camera *this, float rotation);

#endif
