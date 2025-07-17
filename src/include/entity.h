#ifndef RAYCASTER_ENTITY_INCLUDED
#define RAYCASTER_ENTITY_INCLUDED

#include "types.h"

struct level_data;
struct sector;

typedef enum entity_type {
  ENTITY_CAMERA,
  ENTITY_LIGHT
} entity_type;

typedef struct entity {
  struct level_data *level;
  struct sector *sector;
  void *data;
  vec2f position,
        direction;
  float z;
  entity_type type;
} entity;

M_INLINED vec3f
entity_world_position(const entity *this)
{
  return VEC3F(this->position.x, this->position.y, this->z);
}

#endif
