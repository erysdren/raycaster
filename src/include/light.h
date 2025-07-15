#ifndef RAYCASTER_LIGHT_INCLUDED
#define RAYCASTER_LIGHT_INCLUDED

#include "types.h"

#define MAX_LIGHTS_PER_SURFACE 4

struct sector;
struct level_data;

void
level_data_update_lights(struct level_data*);

typedef struct light {
  vec3f position;
  float radius,
        radius_sq,
        radius_sq_inverse;
  float strength;
  struct level_data *level;
} light;

void
light_set_position(light *this, vec3f position);

#endif
