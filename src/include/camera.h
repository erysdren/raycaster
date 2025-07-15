#ifndef RAYCAST_CAMERA_INCLUDED
#define RAYCAST_CAMERA_INCLUDED

#define MIN_CAMERA_PITCH -1.0f
#define MAX_CAMERA_PITCH 1.0f

#include "types.h"
#include "level_data.h"

typedef struct {
  vec2f position,
        direction,
        plane;
  float fov, z, pitch;
  level_data *level;
  sector *in_sector;
} camera;

void
camera_init(camera *this, level_data *level);

void
camera_move(camera *this, float movement);

void
camera_rotate(camera *this, float rotation);

void
camera_set_fov(camera*, float);

#endif
