#ifndef RAYCASTER_LIGHT_INCLUDED
#define RAYCASTER_LIGHT_INCLUDED

#include "entity.h"

#define MAX_LIGHTS_PER_SURFACE 4

void
level_data_update_lights(struct level_data*);

typedef struct light {
  entity entity;
  float radius,
        radius_sq,
        radius_sq_inverse;
  float strength;
} light;

void
light_set_position(light *this, vec3f position);

#endif
