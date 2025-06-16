#include "camera.h"
#include <math.h>
#include <stdio.h>

static void find_current_sector(camera *this);

void camera_init(camera *this, level_data *level) {
  this->level = level;
  this->fov = 0.9;
  this->z = 64;
  this->position = vec2f_make(200, 200);
  this->direction = vec2f_make(1, 0);
  this->plane = vec2f_make(0.f * this->fov, -1.f * this->fov);

  find_current_sector(this);
}

void camera_move(camera *this, float distance) {
  this->position = vec2f_add(this->position, vec2f_mul(this->direction, distance));

  if (!sector_point_inside(this->in_sector, this->position)) {
    find_current_sector(this);
  }
}

void camera_rotate(camera *this, float rotation) {
  const float oldDirX = this->direction.x;
  const float oldPlaneX = this->plane.x;

  this->direction.x = (this->direction.x * cos(rotation)) - (this->direction.y * sin(rotation));
  this->direction.y = (oldDirX * sin(rotation)) + (this->direction.y * cos(rotation));
  this->plane.x = (this->plane.x * cos(rotation)) - (this->plane.y * sin(rotation));
  this->plane.y = (oldPlaneX * sin(rotation)) + (this->plane.y * cos(rotation));
}

static void find_current_sector(camera *this) {
  register size_t i;

  for (i = 0; i < this->level->sectors_count; ++i) {
    if (&this->level->sectors[i] == this->in_sector) { continue; }
    // printf("Check sector %d\n", i);
    if (sector_point_inside(&this->level->sectors[i], this->position)) {
      this->in_sector = (sector *)&this->level->sectors[i];
      printf("Camera entered sector: %d\n", i);
      break;
    }
  }
}