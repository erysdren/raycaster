#include "light.h"
#include "level_data.h"

void light_set_position(light *this, vec3f position) {
  vec3f previous_position = this->position;
  this->position = position;
  level_data_update_lights(this->level);
  map_cache_process_light(&this->level->cache, this, previous_position);
}
